/**
 * @file FunctionChecker.cpp
 * @brief Implementation of anonymous function expression checking.
 * 
 * Handles: AnonFuncExprAST
 */

#include "ExprChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkAnonFuncExpr(AnonFuncExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkAnonFuncExpr");
    
    if (expr->funcType) {
        TypeAST* resolved = ctx.typeResolver->resolve(expr->funcType);
        if (!resolved) return nullptr;
        
        expr->funcType = resolved->as<FuncTypeAST>();
        return expr->funcType;
    }
    
    ctx.error(expr->loc, DiagCode::E2001, "anonymous function has no type");
    return nullptr;
}