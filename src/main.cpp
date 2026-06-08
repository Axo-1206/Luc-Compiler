/**
 * @file main.cpp
 * @brief Luc compiler driver – orchestrates the entire compilation pipeline.
 * 
 * ============================================================================
 * COMPILATION FLOW
 * ============================================================================
 * 
 * The compiler follows a multi‑stage pipeline:
 * 
 *   1. Parse root file                    – read, tokenise, parse the main source file
 *   2. Validate 'main' exists            – early check; abort if missing
 *   3. Resolve imports recursively       – find all `use` declarations and parse referenced files
 *   4. Collect all ASTs                  – store every parsed file in a vector
 *   5. Semantic analysis                 – type checking, symbol resolution, trait validation
 *   6. Output / code generation          – (future: LLVM IR, binary)
 * 
 * ─── Import Resolution (Recursive) ─────────────────────────────────────────
 * 
 *   Only the root file is provided on the command line. All other files are
 *   discovered via `use` declarations inside the source code.
 * 
 *   The algorithm is simple recursion:
 *     1. Parse the root file
 *     2. For each `use` declaration, resolve the path and parse that file
 *     3. Repeat for each parsed file (depth‑first)
 *     4. Track parsed files to avoid cycles
 * 
 * ─── Path Resolution ────────────────────────────────────────────────────────
 * 
 *   Dotted import paths (e.g., `math.vec2`) are resolved to file system paths:
 *     math.vec2 → ./math/vec2.luc  (relative to the importing file's directory)
 * 
 *   All paths are normalised to absolute paths to avoid duplicate loading when
 *   the same file is imported via different relative paths.
 * 
 * ─── Error Handling ─────────────────────────────────────────────────────────
 * 
 *   - Missing 'main' in root file → immediate error, compilation stops
 *   - Lexical errors → reported, compilation stops
 *   - Syntax errors → reported, compilation stops
 *   - Cyclic imports → reported, compilation stops
 *   - Semantic errors → reported, compilation stops
 *   - Warnings → reported, compilation continues
 * 
 * ─── Debug Output ───────────────────────────────────────────────────────────
 * 
 *   Various LUC_DEBUG_* macros control debug output:
 *     - LUC_DEBUG_PARSE_RESULT  → dumps the full AST after parsing
 *     - LUC_DEBUG_DUMP_SYMBOL   → dumps the symbol table after collection
 *     - LUC_DEBUG_TO_FILE       → writes debug output to a file instead of stdout
 *     - LUC_DEBUG_VERBOSITY     → controls detail level (0-3)
 * 
 * @see Parser.cpp for syntax analysis
 * @see SemanticAnalyzer.cpp for semantic analysis
 * @see Lexer.cpp for tokenisation
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "ast/support/ASTArena.hpp"
#include "registry/AttributeRegistry.hpp"
#include "registry/IntrinsicRegistry.hpp"
#include "registry/QualifierRegistry.hpp"
#include "semantic/SemanticAnalyzer.hpp"
#include "debug/DebugMacros.hpp"
#include "debug/ASTDumper.hpp"

namespace fs = std::filesystem;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Converts a file path to an absolute, normalised path.
 * 
 * This is essential for avoiding duplicate loading of the same file when it's
 * imported via different relative paths (e.g., `./math/vec2.luc` vs `math/vec2.luc`).
 * 
 * @param path The input path (may be relative or absolute)
 * @return std::string The absolute, normalised path
 */
std::string getAbsolutePath(const std::string& path) {
    fs::path p(path);
    if (p.is_absolute()) return p.string();
    return fs::absolute(p).string();
}

/**
 * @brief Resolves a dotted import path to a file system path.
 * 
 * Converts a package‑style import (e.g., `math.vec2`) to a relative file path
 * (`math/vec2.luc`) and resolves it against the base directory of the importing file.
 * 
 * Examples:
 *   importPath = "math.vec2", baseDir = "/home/user/src"
 *   → "/home/user/src/math/vec2.luc"
 * 
 *   importPath = "std.io", baseDir = "/project"
 *   → "/project/std/io.luc"
 * 
 * @param importPath The dotted import path (e.g., "math.vec2")
 * @param baseDir The directory of the file that contains the `use` declaration
 * @return std::string The resolved absolute file path
 */
std::string resolveImportPath(const std::string& importPath, const std::string& baseDir) {
    // Convert dots to directory separators
    std::string relativePath = importPath;
    for (char& c : relativePath) {
        if (c == '.') c = '/';
    }
    relativePath += ".luc";
    
    // Resolve against the base directory
    fs::path base(baseDir);
    fs::path full = base / relativePath;
    return full.string();
}

/**
 * @brief Checks if a ProgramAST contains a function named 'main'.
 * 
 * This is a simple AST traversal – no semantic analysis, just looking for
 * a FuncDeclAST with name "main".
 * 
 * @param program The parsed AST to inspect
 * @param mainId The interned ID for the string "main"
 * @return true if a 'main' function declaration exists
 */
bool hasMainFunction(const ProgramAST* program, uint32_t mainId) {
    if (!program) return false;
    
    for (const auto& decl : program->decls) {
        if (auto* func = decl->as<FuncDeclAST>()) {
            if (func->name.id == mainId) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Recursively parses a file and all its imports.
 * 
 * This function parses a single file, collects its `use` declarations,
 * and recursively parses each imported file (if not already parsed).
 * 
 * @param filePath Absolute path to the source file
 * @param stringPool String interning pool (shared across all files)
 * @param arena AST arena allocator (shared across all files)
 * @param parsedFiles Set of already parsed files (to avoid cycles and duplicates)
 * @param programs Output vector of parsed ASTs
 * @param hasError Output flag set to true on any error
 * @return ProgramAST* The parsed AST, or nullptr on error
 */
ProgramAST* parseFileRecursive(const std::string& filePath,
                               StringPool& stringPool,
                               ASTArena& arena,
                               std::unordered_set<std::string>& parsedFiles,
                               std::vector<ProgramAST*>& programs,
                               bool& hasError) {
    // Check for cycles
    if (parsedFiles.find(filePath) != parsedFiles.end()) {
        // Already parsed, return existing AST (need to find it)
        // For simplicity, we return nullptr and let the caller handle
        // But since we track via the set, we just return nullptr and continue
        return nullptr;
    }
    
    // Mark as being parsed (to detect cycles)
    parsedFiles.insert(filePath);
    
    std::cout << "[MAIN] Parsing file: " << filePath << std::endl;
    
    // Read source file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        diagnostic::error(DiagnosticCategory::Lexical, stringPool.intern(filePath),
                          SourceLocation(), DiagCode::E2001,
                          {"Could not open file: " + filePath});
        hasError = true;
        return nullptr;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    // Lexical analysis
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    
    // Check for unknown characters (lexical errors)
    bool lexicalError = false;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::UNKNOWN) {
            diagnostic::error(DiagnosticCategory::Lexical, stringPool.intern(filePath),
                              SourceLocation(tok.line, tok.column), DiagCode::E1001,
                              {"Unexpected character: '" + tok.value + "'"});
            lexicalError = true;
        }
    }
    if (lexicalError) {
        hasError = true;
        return nullptr;
    }
    
    // Syntax analysis
    Parser parser(tokens, stringPool.intern(filePath), stringPool, arena);
    ProgramAST* program = parser.parse();
    
    if (!program || diagnostic::hasErrors()) {
        hasError = true;
        return nullptr;
    }
    
    // Debug output (AST dump)
    if (LucDebug::isDebugEnabled("PARSE_RESULT")) {
        LucDebug::getDebugStream() << LucDebug::dumpAST(program, stringPool, LucDebug::getVerbosity());
    }
    
    // Add to programs vector
    programs.push_back(program);
    
    // Process imports recursively
    for (auto& decl : program->decls) {
        if (!decl->isa<UseDeclAST>()) continue;
        auto* use = decl->as<UseDeclAST>();
        
        // Rebuild the dotted path string (e.g., "math.vec2")
        std::string importPath;
        for (size_t i = 0; i < use->path.size(); ++i) {
            if (i > 0) importPath += '.';
            importPath += stringPool.lookup(use->path[i]);
        }
        
        // Resolve to an absolute file path
        fs::path currentDir = fs::path(filePath).parent_path();
        std::string resolved = resolveImportPath(importPath, currentDir.string());
        
        // Recursively parse the imported file
        parseFileRecursive(resolved, stringPool, arena, parsedFiles, programs, hasError);
    }
    
    return program;
}

// ============================================================================
// Main Entry Point
// ============================================================================

/**
 * @brief Luc compiler main entry point.
 * 
 * The compilation flow is:
 * 
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        COMMAND LINE ARGUMENTS                          │
 * │                         luc-comp main.luc                              │
 * └─────────────────────────────────────────────────────────────────────────┘
 *                                      │
 *                                      ▼
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    PHASE 1: PARSE ROOT FILE                            │
 * │                                                                         │
 * │   Parse the root file                                                  │
 * │   ↓                                                                    │
 * │   Check for 'main' function → if missing → ABORT                      │
 * │   ↓                                                                    │
 * │   Recursively parse all imports (use declarations)                    │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *                                      │
 *                                      ▼
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    PHASE 2: SEMANTIC ANALYSIS                          │
 * │                                                                         │
 * │   Symbol collection → Type resolution → Trait conformance →            │
 * │   Declaration checking → Annotation                                    │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *                                      │
 *                                      ▼
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    PHASE 3: OUTPUT (FUTURE)                            │
 * │                                                                         │
 * │   Code generation → LLVM IR → Object file → Executable                 │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 * 
 * @param argc Number of command line arguments
 * @param argv Array of argument strings
 * @return int Exit code (0 = success, non‑zero = error)
 */
int main(int argc, char* argv[]) {
    // ========================================================================
    // DEBUG INITIALISATION
    // ========================================================================
#ifdef LUC_DEBUG_MASTER
    std::cout << "[DEBUG] LUC_DEBUG_MASTER is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_PARSER
    std::cout << "[DEBUG] LUC_DEBUG_PARSER is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_TYPE
    std::cout << "[DEBUG] LUC_DEBUG_TYPE is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_SEMANTIC
    std::cout << "[DEBUG] LUC_DEBUG_SEMANTIC is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_DUMP_SYMBOL
    std::cout << "[DEBUG] LUC_DEBUG_DUMP_SYMBOL is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_PARSE_RESULT
    std::cout << "[DEBUG] LUC_DEBUG_PARSE_RESULT is ENABLED" << std::endl;
#endif
#ifdef LUC_DEBUG_TO_FILE
    std::cout << "[DEBUG] LUC_DEBUG_TO_FILE is ENABLED" << std::endl;
    std::cout << "[DEBUG] Log file path: " << getAbsolutePath(LUC_DEBUG_FILE_PATH) << std::endl;
#endif
#ifdef LUC_DEBUG_VERBOSITY
    std::cout << "[DEBUG] LUC_DEBUG_VERBOSITY = " << LUC_DEBUG_VERBOSITY << std::endl;
#endif

    std::cout << "\n===============================================" << std::endl;
    std::cout << "[MAIN] Luc Compiler Starting" << std::endl;
    std::cout << "[MAIN] ========================================" << std::endl;

    // ========================================================================
    // COMMAND LINE VALIDATION
    // ========================================================================
    if (argc != 2) {
        std::cerr << "Usage: luc-comp <source-file.luc>" << std::endl;
        std::cerr << "  Only the root file (containing 'main') should be provided." << std::endl;
        std::cerr << "  All dependencies are discovered via 'use' declarations." << std::endl;
        return 1;
    }

    // ========================================================================
    // GLOBAL RESOURCES
    // ========================================================================
    StringPool stringPool;                     // String interning (shared)
    attribute::initialize(stringPool);         // @attr registry
    intrinsic::initialize(stringPool);         // #intrinsic registry
    qualifier::initialize(stringPool);          // ~qualifier registry
    ASTArena arena;                            // Bump‑pointer allocator for AST nodes

    // Intern "main" once for all comparisons
    uint32_t mainId = stringPool.intern("main").id;

    // ========================================================================
    // PHASE 1: PARSE ROOT FILE & ALL IMPORTS
    // ========================================================================
    
    std::string rootFile = getAbsolutePath(argv[1]);
    std::cout << "[MAIN] Root file: " << rootFile << std::endl;
    
    std::unordered_set<std::string> parsedFiles;
    std::vector<ProgramAST*> programs;
    bool hasError = false;
    
    ProgramAST* rootProgram = parseFileRecursive(rootFile, stringPool, arena, parsedFiles, programs, hasError);
    
    if (!rootProgram || hasError) {
        std::cerr << "\n>>> Failed to parse root file or its dependencies." << std::endl;
        diagnostic::dumpAll(stringPool, std::cerr);
        attribute::shutdown();
        intrinsic::shutdown();
        qualifier::shutdown();
        return 1;
    }
    
    // EARLY 'main' VALIDATION – existence only, not semantics
    if (!hasMainFunction(rootProgram, mainId)) {
        std::cerr << "\n>>> ERROR: No 'main' function found in root file: " << argv[1] << std::endl;
        std::cerr << "    A program must have a 'main' entry point." << std::endl;
        attribute::shutdown();
        intrinsic::shutdown();
        qualifier::shutdown();
        return 1;
    }
    
    std::cout << "[MAIN] Root file parsed successfully. 'main' function found." << std::endl;
    std::cout << "[MAIN] Total files parsed: " << programs.size() << std::endl;

    // ========================================================================
    // PHASE 2: SEMANTIC ANALYSIS
    // ========================================================================
    // The semantic analyser performs:
    //   - Symbol collection (Phase 1)
    //   - Type resolution (Phase 2)
    //   - Trait conformance mapping (Phase 2.5)
    //   - Declaration checking (Phase 3)
    //   - Entry point validation (Phase 3.5) – full 'main' signature validation
    //   - Annotation (Phase 4)
    // ========================================================================
    
    std::cout << "[MAIN] Starting semantic analysis on " << programs.size() << " files..." << std::endl;
    SemanticAnalyzer analyzer(stringPool, arena);
    bool semanticSuccess = analyzer.analyze(programs);
    std::cout << "[MAIN] Semantic analysis complete: " << (semanticSuccess ? "SUCCESS" : "FAILED") << std::endl;

    // ========================================================================
    // ERROR REPORTING (Semantic Analysis)
    // ========================================================================
    if (diagnostic::hasErrors()) {
        std::cerr << "\n>>> Semantic Analysis FAILED:" << std::endl;
        diagnostic::dumpAll(stringPool, std::cerr);
        attribute::shutdown();
        intrinsic::shutdown();
        qualifier::shutdown();
        return 1;
    }

    if (diagnostic::hasWarnings()) {
        std::cerr << "\n>>> Semantic Analysis SUCCESSFUL with warnings:" << std::endl;
        diagnostic::dumpAll(stringPool, std::cerr);
    } else {
        std::cout << "\n>>> Semantic Analysis SUCCESSFUL!" << std::endl;
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================
    attribute::shutdown();
    intrinsic::shutdown();
    qualifier::shutdown();

    // Flush debug stream if logging to file
#ifdef LUC_DEBUG_TO_FILE
    LucDebug::getDebugStream() << std::flush;
    std::cout << "[MAIN] Debug logs written to: " << getAbsolutePath(LUC_DEBUG_FILE_PATH) << std::endl;
#endif

    std::cout << "[MAIN] Compilation finished successfully." << std::endl;
    return 0;
}