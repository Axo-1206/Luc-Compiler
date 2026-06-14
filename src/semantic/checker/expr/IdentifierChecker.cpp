/**
 * @file IdentifierChecker.cpp
 * @brief Implementation of identifier expression checking.
 * 
 * Handles: IdentifierExprAST
 */

#include "ExprChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkIdentifierExpr(IdentifierExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkIdentifierExpr: " << ctx.pool.lookup(expr->name));
    
    // First check value namespace
    ValueDeclAST* decl = ctx.scope.lookupValue(expr->name);
    if (decl) {
        if (auto* var = decl->as<VarDeclAST>()) {
            return var->valueType;
        }
        if (auto* func = decl->as<FuncDeclAST>()) {
            return func->funcType;
        }
        if (auto* param = decl->as<ParamAST>()) {
            return param->valueType;
        }
        if (auto* field = decl->as<FieldDeclAST>()) {
            return field->valueType;
        }
        if (auto* variant = decl->as<EnumVariantAST>()) {
            return ctx.scope.lookupType(variant->name)->selfType;
        }
        
        ctx.error(expr->loc, DiagCode::E2001,
                  "cannot determine type of '", ctx.pool.lookup(expr->name), "'");
        return nullptr;
    }
    
    // Then check type namespace (type used as value, e.g., int("42"))
    TypeDeclAST* typeDecl = ctx.scope.lookupType(expr->name);
    if (typeDecl) {
        return typeDecl->selfType;
    }
    
    ctx.error(expr->loc, DiagCode::E2001,
              "undefined identifier '", ctx.pool.lookup(expr->name), "'");
    return nullptr;
}