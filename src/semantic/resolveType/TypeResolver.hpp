/**
 * @file TypeResolver.hpp
 * @brief Resolves type names and annotations using switch‑case dispatch.
 */

#pragma once

#include "ast/TypeAST.hpp"
#include "ast/DeclAST.hpp"
#include <unordered_map>

struct SemanticContext;  // forward declaration

class TypeResolver {
public:
    // No constructor arguments – all methods take SemanticContext&
    TypeResolver() = default;

    // Main entry point
    TypeAST* resolveType(TypeAST* typeNode, SemanticContext& ctx);

    // Declaration‑level helpers
    void resolveStructFields(StructDeclAST& node, SemanticContext& ctx);
    void resolveFunctionType(FuncTypeAST& type, SemanticContext& ctx);
    void resolveFunctionReturnTypes(const FuncTypeAST& type, SemanticContext& ctx);
    TypeAST* getFunctionReturnType(const FuncTypeAST& type, const SourceLocation* loc, SemanticContext& ctx);
    std::vector<TypeAST*> getFunctionReturnTypes(const FuncTypeAST& type, SemanticContext& ctx);

    // Generic parameter stack management (context‑independent)
    void pushGenericParams(const ArenaSpan<GenericParamPtr>* params);
    void popGenericParams();
    bool isGenericParam(InternedString name) const;

    // Substitution map (for generic instantiation)
    void pushSubstitutionMap(const std::unordered_map<InternedString, TypeAST*>* map);
    void popSubstitutionMap();
    TypeAST* lookupSubstitution(InternedString name) const;

    // Constraint checking (uses ctx for diagnostics)
    bool satisfiesConstraints(TypeAST* type, const std::vector<InternedString>& requiredTraits,
                              SemanticContext& ctx) const;

    // Cloning utilities (context‑independent, uses ctx.arena for allocation)
    TypeAST* cloneType(const TypeAST* type, SemanticContext& ctx);
    FuncTypeAST* cloneFuncSignature(const FuncSignature& sig, const SourceLocation& loc, SemanticContext& ctx);

    // Trait mapping (set by SemanticAnalyzer)
    void setStructTraits(const std::unordered_map<InternedString, std::vector<InternedString>>* map) {
        structTraits_ = map;
    }

private:
    // Dispatch helpers (each takes ctx)
    TypeAST* resolvePrimitiveType(PrimitiveTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveNamedType(NamedTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveNullableType(NullableTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveResultType(ResultTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveArrayType(ArrayTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveRefType(RefTypeAST& node, SemanticContext& ctx);
    TypeAST* resolvePtrType(PtrTypeAST& node, SemanticContext& ctx);
    TypeAST* resolveFuncType(FuncTypeAST& node, SemanticContext& ctx);

    void resolveGenericParamConstraints(GenericParamAST& gp, SemanticContext& ctx);

    // Generic stacks (do not depend on ctx)
    std::vector<const ArenaSpan<GenericParamPtr>*> genericParamsStack_;
    std::vector<const std::unordered_map<InternedString, TypeAST*>*> substitutionMapStack_;
    const std::unordered_map<InternedString, std::vector<InternedString>>* structTraits_ = nullptr;
};