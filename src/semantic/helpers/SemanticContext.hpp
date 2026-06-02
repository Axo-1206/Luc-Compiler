/**
 * @file SemanticContext.hpp
 * @brief Plain data container for all semantic analysis state.
 *
 * SemanticContext holds references and flags needed during semantic passes.
 * It is passed by reference to every semantic function. It does NOT own
 * any components; ownership remains with SemanticAnalyzer.
 */

#pragma once

#include "ast/support/StringPool.hpp"
#include "ast/support/ASTArena.hpp"
#include "diagnostics/Diagnostic.hpp"
#include "semantic/SymbolTable.hpp"

struct SemanticContext {
    // ── References to shared resources ──────────────────────────────────────
    StringPool&   pool;      // String pool (for name demangling)
    ASTArena&     arena;     // Arena for temporary type synthesis
    SymbolTable*  symbols;   // Symbol table (non‑owning, owned by SemanticAnalyzer)

    // ── Per‑file state (set before processing each file) ────────────────────
    InternedString currentFile;

    // ── Mutable flags for checking phase ────────────────────────────────────
    int  loopDepth     = 0;
    int  parallelDepth = 0;
    bool insideExtern  = false;

    /**
     * @brief Constructor – binds references to required resources.
     * @param p String pool
     * @param a AST arena
     * @param sym Symbol table (pointer, may be null initially)
     */
    SemanticContext(StringPool& p, ASTArena& a, SymbolTable* sym = nullptr)
        : pool(p), arena(a), symbols(sym) {}

    // ── Convenience diagnostic helpers (using global diagnostic module) ─────
    void error(SourceLocation loc, DiagCode code,
               std::initializer_list<std::string> args = {}) const {
        diagnostic::error(DiagnosticCategory::Semantic, currentFile, loc, code, args);
    }

    void warning(SourceLocation loc, DiagCode code,
                 std::initializer_list<std::string> args = {}) const {
        diagnostic::warning(DiagnosticCategory::Semantic, currentFile, loc, code, args);
    }

    void note(SourceLocation loc, const std::string& msg) const {
        diagnostic::note(currentFile, loc, msg);
    }

    // ── Depth counter helpers ──────────────────────────────────────────────
    void enterLoop()   { ++loopDepth; }
    void exitLoop()    { --loopDepth; }
    void enterParallel(){ ++parallelDepth; }
    void exitParallel() { --parallelDepth; }
    void enterExtern()  { insideExtern = true; }
    void exitExtern()   { insideExtern = false; }
};