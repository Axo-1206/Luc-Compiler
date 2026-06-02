/**
 * @file SemanticAnalyzer.hpp
 * @brief Orchestrates the four passes of semantic analysis.
 */

#pragma once

#include "helpers/SemanticContext.hpp"
#include "collectors/SemanticCollector.hpp"
#include "resolveType/TypeResolver.hpp"
#include "resolveType/TypeChecker.hpp"
#include "SymbolTable.hpp"
#include "ast/BaseAST.hpp"

#include <memory>
#include <vector>

enum class CompilationMode { AOT, JIT };

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(StringPool& pool, ASTArena& arena);

    bool analyze(std::vector<ProgramAST*>& files);
    void dumpSymbols() const;
    CompilationMode getCompilationMode() const { return compilationMode_; }

private:
    // Owned components
    SymbolTable symbols_;
    TypeResolver resolver_;
    TypeChecker checker_;
    SemanticCollector collector_;

    // Context used during analysis (references to above components)
    SemanticContext ctx_;

    // Result of analysis
    CompilationMode compilationMode_ = CompilationMode::AOT;

    // Phase methods
    void resolveImports(std::vector<ProgramAST*>& files);
    void collectSymbols(std::vector<ProgramAST*>& files);
    void resolveTypes(std::vector<ProgramAST*>& files);
    void checkDecls(std::vector<ProgramAST*>& files);
    void annotate(std::vector<ProgramAST*>& files);
    void validateNoDuplicateSymbols();

    // Helper to set current file in context before processing each file
    void setCurrentFile(InternedString file) { ctx_.currentFile = file; }
};