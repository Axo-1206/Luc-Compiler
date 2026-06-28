/**
 * @file ParserContext.hpp
 * @brief Shared parsing context across all files.
 * 
 * ParserContext holds state that is shared across all files being parsed:
 * - StringPool and ASTArena (shared memory)
 * - ModuleResolver (module coordination)
 * - Error reporting (variadic template functions)
 * - Context tracking (spawnDepth, inAsyncContext)
 * 
 * This is passed by reference to all parsing functions.
 */

#pragma once

#include "core/Tokens.hpp"
#include "core/ast/BaseAST.hpp"
#include "core/ast/DeclAST.hpp"
#include "core/memory/ASTArena.hpp"
#include "core/memory/StringPool.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/diagnostics/DiagnosticCodes.hpp"
#include "TokenStream.hpp"
#include "ModuleResolver.hpp"

#include <vector>
#include <string>
#include <optional>
#include <sstream>
#include <type_traits>

namespace parser {

/**
 * @brief Shared parsing context across all files.
 * 
 * ParserContext holds all state that is shared across files:
 * - Memory allocators (StringPool, ASTArena)
 * - Module resolver (imports, caching, circular detection)
 * - Error reporting with variadic templates
 * - Context tracking (spawn/async depth)
 * 
 * ## Error Reporting API
 * 
 * All error reporting functions automatically use the current token location
 * from the TokenStream. The API is:
 * 
 * ```cpp
 * // Simple error at current token location
 * ctx.error(DiagCode::E1002, "expected", "found");
 * 
 * // Error at a specific location (rarely needed)
 * ctx.errorAt(loc, DiagCode::E1002, "expected", "found");
 * 
 * // Warning at current token location
 * ctx.warning(DiagCode::W0001, "message");
 * 
 * // Note at current token location (no code)
 * ctx.note("Informational message");
 * ```
 * 
 * ## Usage
 * 
 * ```cpp
 * ParserContext ctx(pool, arena, resolver);
 * TokenStream stream(tokens, filePath);
 * 
 * // Parse a file - ctx is shared across all recursive parses
 * auto* ast = parse("main.lucid", source, ctx);
 * ```
 */
struct ParserContext {
    // ─────────────────────────────────────────────────────────────────────────
    // Shared Resources
    // ─────────────────────────────────────────────────────────────────────────

    /// String interner (shared across all files)
    StringPool& pool;
    
    /// AST allocator (shared across all files)
    ASTArena& arena;
    
    /// Module resolver (for imports, caching, circular detection)
    ModuleResolver* resolver = nullptr;

    /// File path of the current file being parsed (for error reporting)
    InternedString currentFilePath;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Error Tracking
    // ─────────────────────────────────────────────────────────────────────────
    
    /// True if any error has been reported during parsing
    bool hasErrors = false;
    
    /// Collected diagnostic messages for this file
    std::vector<Diagnostic> errors;
    
    /// Consecutive error count (used to prevent infinite loops in lists)
    int consecutiveErrors = 0;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Context Tracking (shared across all files)
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Depth of spawn/join nesting (OS thread parallelism).
     * 
     * Tracks how deeply we're nested in spawn/join operations.
     * Used to enforce thread-safety rules.
     */
    int spawnDepth = 0;
    
    /**
     * @brief True if currently parsing inside an async context.
     * 
     * Tracks that we're in a function that can use await.
     * Used to validate async/await pairing.
     */
    bool inAsyncContext = false;
    
    /// Current declaration context
    enum class Context {
        TopLevel,   // File-level declarations
        Local,      // Inside a block (function body, etc.)
        Function,   // Inside a function body (return allowed)
        Loop,       // Inside a loop body (break/continue allowed)
        Spawn,      // Inside a spawned thread (spawn restrictions)
        Async,      // Inside an async operation (await allowed)
    };
    Context context = Context::TopLevel;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Doc Comment Harvesting
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Last harvested doc comment (stored between harvest and attachment)
    std::optional<DocComment> pendingDoc;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Constructor
    // ─────────────────────────────────────────────────────────────────────────
    
    ParserContext(StringPool& p, ASTArena& a, ModuleResolver* r = nullptr)
        : pool(p)
        , arena(a)
        , resolver(r)
    {}
    
    // ─────────────────────────────────────────────────────────────────────────
    // String Conversion Helper
    // ─────────────────────────────────────────────────────────────────────────
    
    std::string toString(InternedString s) const {
        return std::string(pool.lookup(s));
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Error Reporting - Variadic Template Functions
    // ─────────────────────────────────────────────────────────────────────────
    
private:
    /**
     * @brief Trait to detect if a type is InternedString.
     */
    template<typename T>
    struct is_interned_string : std::false_type {};
    
    template<>
    struct is_interned_string<InternedString> : std::true_type {};
    
    template<>
    struct is_interned_string<InternedString&> : std::true_type {};
    
    template<>
    struct is_interned_string<const InternedString&> : std::true_type {};
    
    /**
     * @brief Stream any value that is NOT InternedString.
     */
    template<typename T>
    typename std::enable_if<!is_interned_string<typename std::decay<T>::type>::value>::type
    streamTo(std::ostringstream& oss, T&& value) const {
        oss << std::forward<T>(value);
    }
    
    /**
     * @brief Stream InternedString using the StringPool.
     */
    void streamTo(std::ostringstream& oss, InternedString s) const {
        oss << pool.lookup(s);
    }
    
    template<typename T>
    void buildMessageImpl(std::ostringstream& oss, T&& value) const {
        streamTo(oss, std::forward<T>(value));
    }
    
    template<typename T, typename... Rest>
    void buildMessageImpl(std::ostringstream& oss, T&& first, Rest&&... rest) const {
        streamTo(oss, std::forward<T>(first));
        buildMessageImpl(oss, std::forward<Rest>(rest)...);
    }
    
    template<typename... Args>
    std::string buildMessage(Args&&... args) const {
        std::ostringstream oss;
        buildMessageImpl(oss, std::forward<Args>(args)...);
        return oss.str();
    }
    
    void addDiagnostic(DiagnosticSeverity severity, 
                       DiagnosticCategory category,
                       const SourceLocation& loc,
                       DiagCode code,
                       const std::string& message) {
        errors.push_back({
            severity,
            category,
            currentFilePath,
            loc,
            code,
            {message}
        });
        if (severity == DiagnosticSeverity::Error || 
            severity == DiagnosticSeverity::Fatal) {
            hasErrors = true;
            consecutiveErrors++;
        } else if (severity == DiagnosticSeverity::Warning) {
            consecutiveErrors++;
        }
    }
    
public:
    // ─────────────────────────────────────────────────────────────────────────
    // Public Error Reporting API
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Report an error at a specific location.
     * 
     * @tparam Args The types of the format arguments
     * @param loc The source location
     * @param code The diagnostic code
     * @param args The format arguments for the error message
     * 
     * ## Usage Examples
     * 
     * ```cpp
     * // Error at a specific location (rarely needed)
     * ctx.errorAt(loc, DiagCode::E1002, expected, found);
     * ```
     */
    template<typename... Args>
    void errorAt(const SourceLocation& loc, DiagCode code, Args&&... args) {
        addDiagnostic(DiagnosticSeverity::Error, 
                      DiagnosticCategory::Syntax,
                      loc,
                      code,
                      buildMessage(std::forward<Args>(args)...));
    }
    
    /**
     * @brief Report a warning at a specific location.
     */
    template<typename... Args>
    void warningAt(const SourceLocation& loc, DiagCode code, Args&&... args) {
        addDiagnostic(DiagnosticSeverity::Warning, 
                      DiagnosticCategory::Syntax,
                      loc,
                      code,
                      buildMessage(std::forward<Args>(args)...));
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Convenience: Error with location from TokenStream
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Report an error using the current location from a TokenStream.
     * 
     * This is the primary error reporting function used by parser functions.
     * 
     * @tparam Args The types of the format arguments
     * @param stream The token stream to get the current location from
     * @param code The diagnostic code
     * @param args The format arguments for the error message
     * 
     * ## Usage Examples
     * 
     * ```cpp
     * // Error at current token location
     * ctx.error(stream, DiagCode::E1002, expected, found);
     * ```
     */
    template<typename... Args>
    void error(TokenStream& stream, DiagCode code, Args&&... args) {
        addDiagnostic(DiagnosticSeverity::Error, 
                      DiagnosticCategory::Syntax,
                      stream.currentLoc(),
                      code,
                      buildMessage(std::forward<Args>(args)...));
    }
    
    /**
     * @brief Report a warning using the current location from a TokenStream.
     */
    template<typename... Args>
    void warning(TokenStream& stream, DiagCode code, Args&&... args) {
        addDiagnostic(DiagnosticSeverity::Warning, 
                      DiagnosticCategory::Syntax,
                      stream.currentLoc(),
                      code,
                      buildMessage(std::forward<Args>(args)...));
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Context Queries
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Check if we can safely continue parsing.
     */
    bool canContinue() const {
        return consecutiveErrors < 10;
    }
    
    /**
     * @brief Check if we're in a spawn context (parallelism).
     */
    bool isSpawnContext() const { return spawnDepth > 0; }
    
    /**
     * @brief Check if we're in an async context (concurrency).
     */
    bool isAsyncContext() const { return inAsyncContext; }
    
    /**
     * @brief Clear errors for a new file.
     */
    void clearErrors() {
        errors.clear();
        hasErrors = false;
        consecutiveErrors = 0;
    }
};

} // namespace parser