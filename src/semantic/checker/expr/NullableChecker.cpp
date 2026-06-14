/**
 * @file NullableChecker.cpp
 * @brief Implementation of nullable expression checking.
 * 
 * Handles: NullCoalesceExprAST, NullableChainExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkNullCoalesceExpr(NullCoalesceExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkNullCoalesceExpr");
    
    TypeAST* valueType = checkExpr(expr->value, ctx);
    TypeAST* fallbackType = checkExpr(expr->fallback, ctx);
    if (!valueType || !fallbackType) return nullptr;
    
    if (!TypeChecker::isNullable(valueType, *ctx.typeResolver)) {
        ctx.error(expr->value->loc, DiagCode::E2001,
                  "left-hand side of '\?\?' must be nullable");
        return nullptr;
    }
    
    TypeAST* unwrapped = TypeChecker::unwrapNullable(valueType, *ctx.typeResolver);
    if (!TypeChecker::isAssignable(fallbackType, unwrapped, ctx)) {
        ctx.error(expr->fallback->loc, DiagCode::E2001,
                  "fallback value type does not match");
        return nullptr;
    }
    
    return unwrapped;
}

TypeAST* checkNullableChainExpr(NullableChainExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkNullableChainExpr: " << expr->steps.size() << " steps");
    
    TypeAST* currentType = checkExpr(expr->object, ctx);
    if (!currentType) return nullptr;
    
    for (const auto& step : expr->steps) {
        if (!TypeChecker::isNullable(currentType, *ctx.typeResolver)) {
            ctx.error(expr->object->loc, DiagCode::E2001,
                      "cannot chain '?.' on non-nullable type");
            return nullptr;
        }
        
        currentType = TypeChecker::unwrapNullable(currentType, *ctx.typeResolver);
        // TODO: Look up field in struct
    }
    
    return ctx.arena.make<NullableTypeAST>(currentType);
}