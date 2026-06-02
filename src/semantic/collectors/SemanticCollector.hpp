/**
 * @file SemanticCollector.hpp
 * @brief Phase 1: collects top‑level declarations into the symbol table.
 *
 * Uses switch‑case dispatch on ASTKind instead of the visitor pattern.
 */

#pragma once

#include "semantic/helpers/SemanticContext.hpp"
#include "ast/BaseAST.hpp"
#include "ast/DeclAST.hpp"
#include "semantic/SymbolTable.hpp"
#include "diagnostics/Diagnostic.hpp"

struct SemanticContext;  // forward declaration

class SemanticCollector {
public:
    SemanticCollector() = default;

    // Main entry point for a file
    void collectProgram(ProgramAST& program, SemanticContext& ctx);

    // Called after collection to retrieve trait implementation map
    const std::unordered_map<InternedString, std::vector<InternedString>>& getStructTraits() const {
        return structTraits_;
    }

private:
    std::unordered_map<InternedString, std::vector<InternedString>> structTraits_;

    // Dispatch functions – each processes a specific declaration kind
    void collectUseDecl(UseDeclAST& node, SemanticContext& ctx);
    void collectVarDecl(VarDeclAST& node, SemanticContext& ctx);
    void collectFuncDecl(FuncDeclAST& node, SemanticContext& ctx);
    void collectStructDecl(StructDeclAST& node, SemanticContext& ctx);
    void collectEnumDecl(EnumDeclAST& node, SemanticContext& ctx);
    void collectTraitDecl(TraitDeclAST& node, SemanticContext& ctx);
    void collectImplDecl(ImplDeclAST& node, SemanticContext& ctx);
    void collectFromDecl(FromDeclAST& node, SemanticContext& ctx);
    void collectTypeAliasDecl(TypeAliasDeclAST& node, SemanticContext& ctx);

    // Helpers
    void declareSymbol(const Symbol& sym, SemanticContext& ctx);
    void extractExternMetadata(const ArenaSpan<AttributePtr>& attrs, Symbol& sym, SemanticContext& ctx);
    std::string_view getNameString(InternedString name, SemanticContext& ctx) const {
        return ctx.pool.lookup(name);
    }
    bool isDeclared(InternedString name, SemanticContext& ctx) const {
        return ctx.symbols && ctx.symbols->lookup(name) != nullptr;
    }
};