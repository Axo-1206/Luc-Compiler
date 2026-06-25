/**
 * @file Parser.hpp
 * @brief Lucid language parser – converts token streams into AST.
 * 
 * The parser is implemented as a namespace with pure functions.
 * All mutable state is explicitly passed via ParserState.
 */

#pragma once

#include "core/Tokens.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/ast/ExprAST.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "core/diagnostics/Diagnostic.hpp"

#include <vector>
#include <string>
#include <unordered_map>

namespace parser {

// ─────────────────────────────────────────────────────────────────────────────
// TokenStream – Safe token stream abstraction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Wraps a vector of tokens with safe accessors and comment skipping.
 * 
 * TokenStream is the primary interface for consuming tokens during parsing.
 * It automatically skips LINE_COMMENT and DOC_COMMENT tokens, making them
 * invisible to the grammar (they are harvested separately).
 * 
 * ## Usage Example
 * 
 *   TokenStream stream(tokens, filePath);
 *   if (stream.check(TokenType::IDENTIFIER)) {
 *       Token tok = stream.advance();
 *       // use tok
 *   }
 *   stream.consume(TokenType::LBRACE, "expected '{'");
 * 
 * ## Position Management
 * 
 *   - `getPos()` / `setPos()` – for lookahead and error recovery
 *   - `peek()` – inspect without consuming
 *   - `advance()` – consume current token
 */
struct TokenStream {
    // ─── Construction ──────────────────────────────────────────────────
    
    TokenStream() = default;
    TokenStream(std::vector<Token> tokens, const std::string& filePath = "<unknown>");
    
    // ─── Token Consumption ────────────────────────────────────────────
    
    /** @brief Return the current token without consuming it. */
    const Token& peek() const;
    
    /** @brief Consume and return the current token. */
    Token advance();
    
    /** @brief Check if the current token is of the given type. */
    bool check(TokenType type) const;
    
    /** @brief Check if the current token matches any of the given types. */
    bool checkAny(std::initializer_list<TokenType> types) const;
    
    /** @brief If the current token matches, consume and return it. */
    bool match(TokenType type);
    
    /** @brief Consume the current token, expecting it to be of the given type. */
    Token consume(TokenType type);
    
    /** @brief Check if we've reached the end of the token stream. */
    bool isAtEnd() const;
    
    /** @brief Get the current source location. */
    SourceLocation currentLoc() const;
    
    // ─── Lookahead ──────────────────────────────────────────────────────
    
    /** @brief Get the type of the current token. */
    TokenType peekType() const { return peek().type; }
    
    /** @brief Get the type of the next token (skipping comments). */
    TokenType peekNextType() const;
    
    /** @brief Get the next token without consuming it. */
    const Token& peekNext() const;
    
    /** @brief Get a token at an offset from current position. */
    const Token& peekAt(size_t offset) const;
    
    /** @brief Check if a token type is a primitive type. */
    bool isPrimitiveTypeToken(TokenType type) const;
    
    // ─── Position Management ───────────────────────────────────────────
    
    /** @brief Get the current position in the token vector. */
    size_t getPos() const { return pos_; }
    
    /** @brief Set the current position in the token vector. */
    void setPos(size_t pos) { pos_ = pos; }
    
    /** @brief Get the underlying token vector (raw access). */
    const std::vector<Token>& getTokens() const { return tokens_; }
    
    /** @brief Get a token at a specific index (raw access). */
    const Token& getTokenAt(size_t idx) const { return tokens_[idx]; }
    
    /** @brief Get the total number of tokens. */
    size_t getTokenCount() const { return tokens_.size(); }
    
    /** @brief Skip comments from a given position. */
    size_t skipCommentsFrom(size_t start) const;
    
    /** @brief Convert a token to a SourceLocation. */
    SourceLocation locOf(const Token& tok) const;
    
private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::string filePath_;
    
    static const Token eofToken_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParserState – Mutable context for a single parsing session
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Mutable state for a single parsing session.
 * 
 * All parsing functions receive this struct by reference.
 * This makes the parser reentrant and testable.
 * 
 * ## Lifetime
 * 
 * Created once per file being parsed. For imported files, a new
 * ParserState is created with the same pool and arena.
 * 
 * ## Thread Safety
 * 
 * Not thread-safe. Designed for single-threaded parsing.
 * 
 * ## Memory Ownership
 * 
 * - `stream`: Owns the token vector (moved in)
 * - `pool`: Reference to shared StringPool (owned by ProcessingSession)
 * - `arena`: Reference to shared ASTArena (owned by ProcessingSession)
 * - `errors`: Owns diagnostic messages
 * - `importedModules`: Owns imported ASTs (arena-allocated)
 * 
 * ## Usage Example
 * 
 * ```cpp
 * auto tokens = lexer::tokenize(source, path);
 * TokenStream stream(std::move(tokens), pool.intern(path));
 * ParserState state(std::move(stream), pool.intern(path), pool, arena);
 * auto ast = parser::parse(state);
 * if (state.hasErrors) { report(state.errors); }
 * ```
 */
struct ParserState {
    // ─── Core State ──────────────────────────────────────────────────────
    
    /// The token stream being consumed (mutable)
    TokenStream stream;
    
    /// Source file path (for error reporting)
    InternedString filePath;
    
    /// String interner (shared across all files)
    StringPool& pool;
    
    /// AST allocator (shared across all files)
    ASTArena& arena;
    
    // ─── Error Tracking ──────────────────────────────────────────────────
    
    /// True if any error has been reported during parsing
    bool hasErrors = false;
    
    /// Collected diagnostic messages
    std::vector<Diagnostic> errors;
    
    /// Consecutive error count (used to prevent infinite loops in lists)
    int consecutiveErrors = 0;
    
    // ─── Context Tracking ────────────────────────────────────────────────
    
    /// Depth of parallel body nesting (to restrict return/break/continue)
    int parallelDepth = 0;
    
    /// True if currently parsing an async function body (await allowed)
    bool inAsyncBody = false;
    
    /// Current declaration context (top-level vs local)
    enum class Context {
        TopLevel,   // File-level declarations
        Local,      // Inside a block (function body, etc.)
        Function,   // Inside a function body (return allowed)
        Loop,       // Inside a loop body (break/continue allowed)
    };
    Context context = Context::TopLevel;
    
    // ─── Module System ──────────────────────────────────────────────────
    
    /**
     * @brief Map from module path to imported ProgramAST.
     * 
     * Used to resolve symbols from imported modules.
     * The ASTs are arena-allocated and owned by the same arena.
     */
    std::unordered_map<InternedString, ProgramAST*> importedModules;
    
    /**
     * @brief Modules currently being parsed (to detect circular imports).
     * 
     * When parsing a module, its path is pushed onto this stack.
     * If we encounter a use declaration for a path already in the stack,
     * it's a circular import error.
     */
    std::vector<InternedString> parsingStack;
    
    // ─── Doc Comment Harvesting ──────────────────────────────────────────
    
    /// Last harvested doc comment (stored between harvest and attachment)
    std::optional<DocComment> pendingDoc;
    
    // ─── Qualifier Registry ─────────────────────────────────────────────
    
    /// Reference to the global qualifier registry (for validation)
    // QualifierRegistry& qualifiers; // To be added when implemented
    
    // ─── Constructor ─────────────────────────────────────────────────────
    
    /**
     * @brief Create a new parser state.
     * 
     * @param s Token stream (ownership taken)
     * @param path File path (interned)
     * @param p String pool reference
     * @param a AST arena reference
     */
    ParserState(TokenStream&& s, InternedString path, StringPool& p, ASTArena& a)
        : stream(std::move(s))
        , filePath(path)
        , pool(p)
        , arena(a)
    {}
    
    // ─── Convenience Methods ────────────────────────────────────────────
    
    /// Report an error at the current token location
    void error(const std::string& message);
    
    /// Report an error at a specific location
    void error(const SourceLocation& loc, const std::string& message);
    
    /// Report an error with diagnostic code
    void error(SourceLocation loc, DiagCode code, std::initializer_list<std::string> args = {});
    
    /// Check if we can safely continue parsing
    bool canContinue() const { return !hasErrors; }
    
    /// Check if we're in a parallel body
    bool isParallel() const { return parallelDepth > 0; }
    
    /// Check if we're in an async body
    bool isAsync() const { return inAsyncBody; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse a complete translation unit.
 * 
 * @param state Parser state (contains token stream and allocators)
 * @return ProgramAST* Root AST node (arena-allocated), or nullptr on fatal error
 */
ProgramAST* parse(ParserState& state);

/**
 * @brief Parse a single file into a ProgramAST.
 * 
 * Convenience wrapper that creates a ParserState internally.
 * 
 * @param path File path (for error reporting)
 * @param source Source code
 * @param pool String pool (for interning)
 * @param arena AST arena (for allocation)
 * @return ProgramAST* Root AST node, or nullptr on error
 */
ProgramAST* parseFile(const std::string& path, 
                      const std::string& source,
                      StringPool& pool, 
                      ASTArena& arena);

// ─────────────────────────────────────────────────────────────────────────────
// Internal Parser Functions (declared here for organization)
// ─────────────────────────────────────────────────────────────────────────────

// ─── Declarations ────────────────────────────────────────────────────────────

DeclAST* parseTopLevelDecl(ParserState& state);
UseDeclAST* parseUseDecl(ParserState& state);
VarDeclAST* parseVarDecl(ParserState& state);
FuncDeclAST* parseFuncDecl(ParserState& state);
StructDeclAST* parseStructDecl(ParserState& state);
EnumDeclAST* parseEnumDecl(ParserState& state);
TraitDeclAST* parseTraitDecl(ParserState& state);

// ─── Statements ──────────────────────────────────────────────────────────────

StmtAST* parseStmt(ParserState& state);
BlockStmtAST* parseBlock(ParserState& state);
IfStmtAST* parseIfStmt(ParserState& state);
SwitchStmtAST* parseSwitchStmt(ParserState& state);
SwitchCaseAST* parseSwitchCase(ParserState& state);
ForStmtAST* parseForStmt(ParserState& state);
WhileStmtAST* parseWhileStmt(ParserState& state);
DoWhileStmtAST* parseDoWhileStmt(ParserState& state);
ReturnStmtAST* parseReturnStmt(ParserState& state);
BreakStmtAST* parseBreakStmt(ParserState& state);
ContinueStmtAST* parseContinueStmt(ParserState& state);
ExprStmtAST* parseExprStmt(ParserState& state);
DeclStmtAST* parseDeclStmt(ParserState& state);
MultiVarDeclAST* parseMultiVarDecl(ParserState& state);
MultiAssignStmtAST* parseMultiAssignStmt(ParserState& state);

// ─── Expressions (Pratt Parser) ─────────────────────────────────────────────

ExprAST* parseExpr(ParserState& state);
ExprAST* parsePrattExpr(ParserState& state, int minPrec);
ExprAST* parsePrefixExpr(ParserState& state);
ExprAST* parsePrimaryExpr(ParserState& state);
ExprAST* parsePostfixExpr(ParserState& state, ExprPtr lhs);

// ─── Literals ────────────────────────────────────────────────────────────────

LiteralExprAST* parseLiteralExpr(ParserState& state);
ArrayLiteralExprAST* parseArrayLiteralExpr(ParserState& state);
StructLiteralExprAST* parseStructLiteralExpr(ParserState& state, InternedString typeName, ArenaSpan<TypeAST*> genericArgs);
AnonFuncExprAST* parseAnonFuncExpr(ParserState& state);
IntrinsicCallExprAST* parseIntrinsicCallExpr(ParserState& state);
IfExprAST* parseIfExpr(ParserState& state);
RangeExprAST* parseRangeExpr(ParserState& state, ExprPtr lo);

// ─── Concurrency ─────────────────────────────────────────────────────────────

AsyncExprAST* parseAsyncExpr(ParserState& state);
AwaitExprAST* parseAwaitExpr(ParserState& state);
SpawnExprAST* parseSpawnExpr(ParserState& state);
JoinExprAST* parseJoinExpr(ParserState& state);

// ─── Call & Index ────────────────────────────────────────────────────────────

CallExprAST* parseCallExpr(ParserState& state, ExprPtr callee, ArenaSpan<TypeAST*> genericArgs);
IndexExprAST* parseIndexExpr(ParserState& state, ExprPtr target);
SliceExprAST* parseSliceExpr(ParserState& state, ExprPtr target);

// ─── Pipeline & Composition ──────────────────────────────────────────────────

ExprAST* parsePipelineExpr(ParserState& state, ExprPtr seed);
ExprAST* parseComposeExpr(ParserState& state, ExprPtr lhs);
PipelineStepAST* parsePipelineStep(ParserState& state);
ComposeOperandAST* parseComposeOperand(ParserState& state);

// ─── Types ──────────────────────────────────────────────────────────────────

TypeAST* parseType(ParserState& state);
TypeAST* parseBaseType(ParserState& state);
TypeAST* parsePrimitiveType(ParserState& state);
TypeAST* parseNamedType(ParserState& state);
TypeAST* parseGenericParamRef(ParserState& state);
TypeAST* parseArrayType(ParserState& state);
TypeAST* parseRefType(ParserState& state);
TypeAST* parsePtrType(ParserState& state);
TypeAST* parseFuncType(ParserState& state);
TypeAST* parseTypeWithNullable(ParserState& state);

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::optional<DocComment> harvestDocComment(ParserState& state);
ArenaSpan<AttributePtr> parseAttributes(ParserState& state);
AttributePtr parseAttribute(ParserState& state);
AttributeArgPtr parseAttributeArgLiteral(ParserState& state);

ArenaSpan<ExprAST*> parseArgList(ParserState& state);
ArenaSpan<TypeAST*> parseReturnList(ParserState& state);
std::vector<ParamPtr> parseParamList(ParserState& state);
std::vector<InternedString> parseUsePath(ParserState& state);

GenericParamDeclPtr parseGenericParamDecl(ParserState& state);
ArenaSpan<GenericParamDeclPtr> parseGenericParamDecls(ParserState& state);
TypePtr parseGenericArg(ParserState& state);
ArenaSpan<TypePtr> parseGenericArgs(ParserState& state);

FieldDeclPtr parseFieldDecl(ParserState& state);
EnumVariantPtr parseEnumVariant(ParserState& state);
TraitFieldPtr parseTraitField(ParserState& state);
TraitRefPtr parseTraitRef(ParserState& state);

ExprPtr parseLvalue(ParserState& state);
ExprPtr parseFuncRef(ParserState& state);

// ─── Lookahead Helpers ──────────────────────────────────────────────────────

bool isStartOfDeclaration(ParserState& state);
bool isStartOfStatement(ParserState& state);
bool isStartOfType(ParserState& state);
bool looksLikeType(ParserState& state);
bool looksLikeFuncDecl(ParserState& state);
bool looksLikeAnonFunc(ParserState& state);
bool looksLikeStructLiteral(ParserState& state);
bool looksLikeMultiAssignStart(ParserState& state);
bool isFunctionTypeAfterParen(ParserState& state, size_t startPos);

// ─── Precedence Helpers ──────────────────────────────────────────────────────

int infixPrec(TokenType type);
BinaryOp tokenToBinaryOp(TokenType type);
AssignOp tokenToAssignOp(TokenType type);
bool isAssignOp(TokenType type);

// ─── Infix Dispatch ─────────────────────────────────────────────────────────

ExprPtr parseInfixAssign(ParserState& state, ExprPtr lhs);
ExprPtr parseInfixIs(ParserState& state, ExprPtr lhs);
ExprPtr parseInfixNullCoalesce(ParserState& state, ExprPtr lhs);
ExprPtr parseInfixBinary(ParserState& state, ExprPtr lhs, TokenType opTok, int prec);

// ─── Error Recovery ─────────────────────────────────────────────────────────

void synchronize(ParserState& state);
void synchronizeTo(ParserState& state, std::initializer_list<TokenType> stopTokens);

} // namespace parser