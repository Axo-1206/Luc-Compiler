/**
 * @file Parser.cpp
 * @brief Implementation of TokenStream, ParserState, and core parsing functions.
 * 
 * This file implements the core parsing infrastructure:
 * - TokenStream: Safe token consumption with comment skipping
 * - ParserState: Mutable context for a parsing session
 * - Error Recovery: synchronize() and synchronizeTo()
 * - Entry Points: parse() and parseFile()
 */

#include "Parser.hpp"
#include "Lexer.hpp"
#include "debug/DebugMacros.hpp"
#include "debug/DebugUtils.hpp"
#include "core/diagnostics/DiagnosticCodes.hpp"

#include <filesystem>

namespace parser {

// =============================================================================
// Error Recovery Functions
// =============================================================================

/**
 * @brief Synchronize the parser to the next statement or declaration boundary.
 * 
 * This function implements panic-mode error recovery. When a parsing error
 * occurs, the parser skips tokens until it finds a token that could start a
 * new statement or declaration. This prevents cascading errors and allows
 * the parser to continue parsing the rest of the file.
 * 
 * ## Synchronization Tokens
 * 
 * The parser synchronizes to the following tokens:
 * - Control flow: `if`, `switch`, `for`, `while`, `do`, `return`, `break`,
 *   `continue`
 * - Declarations: `let`, `const`, `struct`, `enum`, `trait`, `use`
 * - Blocks: `{`
 * - Special: `;` (statement terminator)
 * 
 * ## Usage
 * 
 * ```cpp
 * if (parseError) {
 *     state.error("Failed to parse expression");
 *     synchronize(state);
 * }
 * ```
 * 
 * @param state The parser state containing the token stream.
 * 
 * @see synchronizeTo() for synchronizing to specific tokens.
 */
void synchronize(ParserState& state) {
    LOG_PARSER_DETAIL("Synchronizing parser");
    
    // Skip tokens until we find a synchronization point
    while (!state.stream.isAtEnd()) {
        TokenType current = state.stream.peekType();
        
        // Check if we've reached a statement or declaration boundary
        switch (current) {
            // Control flow statements
            case TokenType::IF:
            case TokenType::SWITCH:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::DO:
            case TokenType::RETURN:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            // Declarations
            case TokenType::LET:
            case TokenType::CONST:
            case TokenType::STRUCT:
            case TokenType::ENUM:
            case TokenType::TRAIT:
            case TokenType::USE:
            // Block start
            case TokenType::LBRACE:
            // Statement terminator
            case TokenType::SEMICOLON:
                LOG_PARSER_DETAIL("Synchronized at token: %s", 
                           debug::tokenTypeToString(current).c_str());
                return;
            default:
                break;
        }
        
        // Skip this token and continue
        state.stream.advance();
    }
    
    LOG_PARSER_DETAIL("Synchronization reached EOF");
}

/**
 * @brief Synchronize the parser to one of a specific set of tokens.
 * 
 * This function skips tokens until it finds a token that matches any of the
 * specified stop tokens. This is useful for more targeted error recovery,
 * such as synchronizing to a closing brace or a specific keyword.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Synchronize to a closing brace or the next case
 * synchronizeTo(state, {TokenType::RBRACE, TokenType::CASE, TokenType::DEFAULT});
 * ```
 * 
 * @param state      The parser state containing the token stream.
 * @param stopTokens A list of token types to stop at.
 * 
 * @see synchronize() for general error recovery.
 */
void synchronizeTo(ParserState& state, std::initializer_list<TokenType> stopTokens) {
    LOG_PARSER_DETAIL("Synchronizing to stop tokens");
    
    while (!state.stream.isAtEnd()) {
        TokenType current = state.stream.peekType();
        
        // Check if we've reached a stop token
        for (TokenType stop : stopTokens) {
            if (current == stop) {
                LOG_PARSER_DETAIL("Synchronized at token: %s", 
                           debug::tokenTypeToString(current).c_str());
                return;
            }
        }
        
        // Skip this token and continue
        state.stream.advance();
    }
    
    LOG_PARSER_DETAIL("Synchronization reached EOF");
}

// =============================================================================
// Parser Entry Points
// =============================================================================

/**
 * @brief Parse a complete translation unit.
 * 
 * This function is the main entry point for the parser. It consumes tokens
 * from the `ParserState` and produces a `ProgramAST` that represents the
 * entire source file.
 * 
 * ## Parsing Process
 * 
 * 1. **Create ProgramAST**: Allocates the root node in the arena.
 * 2. **Parse Declarations**: Iteratively parses top-level declarations until
 *    the end of the token stream.
 * 3. **Error Recovery**: Detects and recovers from parsing errors using panic
 *    mode and consecutive error detection.
 * 4. **Build AST**: Converts the temporary declaration list to an `ArenaSpan`.
 * 
 * ## Error Handling
 * 
 * - If a declaration fails to parse and no error was reported, a generic
 *   error is emitted and the parser synchronizes to the next safe token.
 * - If more than 10 consecutive errors occur, the parser aborts to prevent
 *   an infinite loop.
 * - Stray semicolons at the top level are silently skipped.
 * 
 * @param state Parser state containing the token stream, allocators, and
 *              error tracking.
 * @return ProgramAST* The root AST node (arena-allocated), or nullptr if
 *         a fatal error occurred.
 * 
 * @see ParserState for state management
 * @see parseTopLevelDecl for declaration parsing
 */
ProgramAST* parse(ParserState& state) {
    LOG_PARSER_MINIMAL("Starting parse of: %s", 
                debug::internedToString(state.pool, state.filePath).c_str());
    
    // ─── 1. Create the Program AST Node ──────────────────────────────────
    // This node will own all top-level declarations in the file.
    auto* program = state.arena.make<ProgramAST>();
    program->filePath = state.filePath;
    
    // ─── 2. Parse Top-Level Declarations ─────────────────────────────────
    // We collect declarations in a vector first, then convert to ArenaSpan.
    std::vector<DeclPtr> decls;
    
    while (!state.stream.isAtEnd()) {
        // Skip any stray semicolons or separators at the top level.
        // These are common in error recovery and should be ignored.
        if (state.stream.check(TokenType::SEMICOLON)) {
            LOG_PARSER_DETAIL("Skipping stray semicolon at top level");
            state.stream.advance();
            continue;
        }
        
        // ─── Parse a top-level declaration ──────────────────────────────
        // The declaration parser handles structs, enums, traits, functions,
        // variables, and use declarations.
        auto* decl = parseTopLevelDecl(state);
        if (decl) {
            decls.push_back(decl);
            LOG_PARSER_DETAIL("Parsed top-level declaration: %s", 
                       debug::kindToString(decl->kind).c_str());
        } else if (!state.hasErrors) {
            // If we got nullptr but no error was reported, something went
            // wrong. Emit a generic error and attempt recovery.
            state.error("Failed to parse top-level declaration");
            synchronize(state);
        }
        
        // ─── Check for consecutive errors ───────────────────────────────
        // This prevents infinite loops when the parser gets stuck in an
        // error state. After 10 consecutive errors, we abort.
        if (state.hasErrors) {
            state.consecutiveErrors++;
            if (state.consecutiveErrors > 10) {
                state.error("Too many consecutive errors, aborting parse");
                LOG_PARSER_MINIMAL("Aborting due to consecutive errors");
                // Return whatever we have so far
                auto builder = state.arena.makeBuilder<DeclPtr>();
                for (auto* d : decls) builder.push_back(d);
                program->decls = builder.build();
                return program;
            }
        } else {
            state.consecutiveErrors = 0;
        }
    }
    
    // ─── 3. Build the AST ─────────────────────────────────────────────────
    // Convert the temporary vector to an ArenaSpan for immutable storage.
    auto builder = state.arena.makeBuilder<DeclPtr>();
    for (auto* d : decls) {
        builder.push_back(d);
    }
    program->decls = builder.build();
    
    LOG_PARSER_MINIMAL("Parse complete: %zu declarations in %s", 
                decls.size(), 
                debug::internedToString(state.pool, state.filePath).c_str());
    
    return program;
}

/**
 * @brief Parse a single source file into a ProgramAST.
 * 
 * This convenience wrapper handles the entire lexing and parsing pipeline:
 * 1. Lex the source code using `lexer::tokenize()`
 * 2. Create a `TokenStream` from the tokens
 * 3. Create a `ParserState` with the token stream
 * 4. Parse the file using `parse()`
 * 
 * ## When to Use This Function
 * 
 * Use this function when you have a source file as a string and want to
 * parse it without manually setting up the lexer and token stream. This is
 * useful for:
 * - The main compiler/interpreter driver
 * - Test harnesses
 * - Tools that process single files
 * 
 * ## When NOT to Use This Function
 * 
 * If you already have a token stream, use `parse()` directly to avoid
 * unnecessary overhead. This function creates a new lexer and token stream
 * every time it's called.
 * 
 * ## Error Handling
 * 
 * Errors from the lexer and parser are collected in the `ParserState` and
 * can be retrieved via `state.hasErrors` and `state.errors`. The function
 * returns nullptr if a fatal error occurred.
 * 
 * @param path   The file path (used for error reporting and interning).
 * @param source The source code as a string.
 * @param pool   The string pool for interning identifiers and strings.
 * @param arena  The AST arena for allocating nodes.
 * @return ProgramAST* The root AST node (arena-allocated), or nullptr if
 *         a fatal error occurred.
 * 
 * @see lexer::tokenize for lexing
 * @see parse for the main parsing function
 */
ProgramAST* parseFile(const std::string& path, 
                      const std::string& source,
                      StringPool& pool, 
                      ASTArena& arena) {
    LOG_PARSER_MINIMAL("Parsing file: %s", path.c_str());
    
    // ─── 1. Lex the Source ──────────────────────────────────────────────
    // The lexer produces a vector of tokens with source locations.
    auto tokens = lexer::tokenize(source, path);
    if (tokens.empty()) {
        LOG_PARSER_MINIMAL("Lexer produced no tokens for: %s", path.c_str());
        return nullptr;
    }
    
    // Check if the lexer encountered any errors (UNKNOWN tokens)
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::UNKNOWN) {
            LOG_PARSER_MINIMAL("Lexer error in: %s", path.c_str());
            return nullptr;
        }
    }
    
    // ─── 2. Create the Token Stream ──────────────────────────────────────
    // The token stream handles comment skipping and lookahead.
    InternedString filePath = pool.intern(path);
    TokenStream stream(std::move(tokens), filePath);
    
    // ─── 3. Create the Parser State ──────────────────────────────────────
    // The state holds all mutable parsing context.
    ParserState state(std::move(stream), filePath, pool, arena);
    
    // ─── 4. Parse the File ──────────────────────────────────────────────
    // This is the main parsing function that produces the AST.
    auto* program = parse(state);
    
    // ─── 5. Report Results ──────────────────────────────────────────────
    if (state.hasErrors) {
        LOG_PARSER_MINIMAL("Parse completed with errors in: %s", path.c_str());
    } else {
        LOG_PARSER_MINIMAL("Parse completed successfully: %s", path.c_str());
    }
    
    return program;
}

} // namespace parser