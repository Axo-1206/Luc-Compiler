/**
 * @file FieldAccessChecker.cpp
 * @brief Implementation of field and method access checking.
 * 
 * Handles: FieldAccessExprAST, BehaviorAccessExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkFieldAccessExpr(FieldAccessExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkFieldAccessExpr: ." << ctx.pool.lookup(expr->field));
    
    TypeAST* objType = checkExpr(expr->object, ctx);
    if (!objType) return nullptr;
    
    objType = TypeChecker::getUnderlyingType(objType, *ctx.typeResolver);
    
    if (auto* named = objType->as<NamedTypeAST>()) {
        TypeDeclAST* typeDecl = ctx.scope.lookupType(named->name);
        if (auto* structDecl = typeDecl->as<StructDeclAST>()) {
            for (auto* field : structDecl->fields) {
                if (field->name == expr->field) {
                    return field->valueType;
                }
            }
        }
    }
    
    ctx.error(expr->loc, DiagCode::E2001,
              "type has no field '", ctx.pool.lookup(expr->field), "'");
    return nullptr;
}

TypeAST* checkBehaviorAccessExpr(BehaviorAccessExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkBehaviorAccessExpr: :" << ctx.pool.lookup(expr->method));
    
    TypeAST* objType = checkExpr(expr->object, ctx);
    if (!objType) return nullptr;
    
    objType = TypeChecker::getUnderlyingType(objType, *ctx.typeResolver);
    
    // TODO: Implement method resolution using trait conformance map
    // For now, return error
    
    ctx.error(expr->loc, DiagCode::E2001,
              "method '", ctx.pool.lookup(expr->method), 
              "' not found for type");
    return nullptr;
}