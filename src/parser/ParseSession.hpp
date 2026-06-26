/**
 * @file ParseSession.hpp
 * @brief Manages parsing of multiple files in a compilation session.
 * 
 * The ParseSession is responsible for:
 * - Parsing source files into ASTs
 * - Resolving module imports
 * - Caching parsed files
 * - Detecting circular imports
 * - Managing shared resources (StringPool, ASTArena)
 * 
 * This is a pure parsing layer - no semantic analysis or code generation.
 * It produces a complete AST for all files in the session.
 * 
 * ## Usage Example
 * 
 * ```cpp
 * // Create a parsing session
 * ParseSession session("./src");
 * 
 * // Parse the entry file (imports resolved automatically)
 * ProgramAST* main = session.parseFile("main.lucid");
 * 
 * // Parse all files in the package
 * session.parseAll();
 * 
 * // Access parsed files
 * auto files = session.getAllParsedFiles();
 * 
 * // Now the AST is ready for semantic analysis or interpretation
 * ```
 */

#pragma once

#include "core/diagnostics/Diagnostic.hpp"
#include "parser/ModuleResolver.hpp"
#include "core/memory/StringPool.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/ast/BaseAST.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

namespace parser {

/**
 * @brief Manages a parsing session across multiple files.
 * 
 * The ParseSession owns all shared resources and coordinates
 * the parsing of multiple files with import resolution.
 * 
 * ## Lifecycle
 * 
 * 1. Create session with package root
 * 2. Parse files (imports are resolved recursively)
 * 3. Access parsed ASTs
 * 4. Session destroyed, all ASTs freed
 * 
 * ## Thread Safety
 * 
 * Not thread-safe. Designed for single-threaded parsing.
 * 
 * ## Memory Management
 * 
 * All AST nodes are allocated in the arena and freed when
 * the session is destroyed.
 */
class ParseSession {
public:
    // ─── Construction ──────────────────────────────────────────────────────
    
    /**
     * @brief Create a new parsing session.
     * 
     * @param packageRoot The root directory of the package
     */
    explicit ParseSession(const std::filesystem::path& packageRoot);
    
    // ─── File Parsing ─────────────────────────────────────────────────────
    
    /**
     * @brief Parse a single file.
     * 
     * This is the main entry point for parsing. If the file imports other
     * modules, they will be parsed recursively (or fetched from cache).
     * 
     * @param filePath The path to the file (relative to package root)
     * @param source Optional source code (if not provided, read from disk)
     * @return ProgramAST* The parsed AST, or nullptr on error
     */
    ProgramAST* parseFile(const std::string& filePath, 
                          const std::string& source = "");
    
    /**
     * @brief Parse all files in the package.
     * 
     * Recursively finds and parses all .lucid files in the package root.
     * Useful for the compiler mode where we want to parse everything
     * before semantic analysis.
     */
    void parseAll();
    
    /**
     * @brief Get a parsed file by its path.
     * 
     * @param filePath The file path (relative to package root)
     * @return ProgramAST* The parsed AST, or nullptr if not found
     */
    ProgramAST* getParsedFile(const std::string& filePath) const;
    
    /**
     * @brief Get all parsed files.
     * 
     * @return std::vector<ProgramAST*> All parsed ASTs
     */
    std::vector<ProgramAST*> getAllParsedFiles() const;
    
    /**
     * @brief Get all parsed file paths.
     * 
     * @return std::vector<std::string> All parsed file paths
     */
    std::vector<std::string> getAllParsedFilePaths() const;
    
    // ─── Module Resolution ────────────────────────────────────────────────
    
    /**
     * @brief Import a module by its use path.
     * 
     * Called by the parser when encountering a use declaration.
     * 
     * @param usePath The import path (e.g., "std.io")
     * @return ProgramAST* The imported module AST, or nullptr on error
     */
    ProgramAST* importModule(const std::string& usePath);
    
    /**
     * @brief Get the module resolver.
     */
    ModuleResolver& getModuleResolver() { return moduleResolver_; }
    
    // ─── Diagnostics ──────────────────────────────────────────────────────
    
    /**
     * @brief Check if any errors occurred during parsing.
     */
    bool hasErrors() const { return !errors_.empty(); }
    
    /**
     * @brief Get all errors.
     */
    const std::vector<Diagnostic>& getErrors() const { return errors_; }
    
    /**
     * @brief Clear errors.
     */
    void clearErrors() { errors_.clear(); }
    
    // ─── Accessors ───────────────────────────────────────────────────────
    
    StringPool& getStringPool() { return pool_; }
    ASTArena& getArena() { return arena_; }
    const StringPool& getStringPool() const { return pool_; }
    const ASTArena& getArena() const { return arena_; }
    
private:
    StringPool pool_;
    ASTArena arena_;
    ModuleResolver moduleResolver_;
    
    // Map from file path (interned) to parsed AST
    std::unordered_map<InternedString, ProgramAST*> parsedFiles_;
    
    // List of all parsed files (in order of parsing)
    std::vector<InternedString> parsedFileOrder_;
    
    // Errors collected during parsing
    std::vector<Diagnostic> errors_;
    
    // Files currently being parsed (for circular import detection)
    std::vector<InternedString> parsingStack_;
    
    // ─── Internal Methods ─────────────────────────────────────────────────
    
    /**
     * @brief Actually parse a file (internal method).
     */
    ProgramAST* parseFileInternal(InternedString filePath, const std::string& source);
    
    /**
     * @brief Read a file from disk.
     */
    std::string readFile(const std::filesystem::path& path) const;
    
    /**
     * @brief Report an error.
     */
    void reportError(const std::string& message, const std::string& file = "");
};

} // namespace parser