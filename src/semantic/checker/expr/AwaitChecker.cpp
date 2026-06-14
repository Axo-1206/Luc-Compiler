/**
 * @file AwaitChecker.cpp
 * @brief Implementation of await expression checking.
 * 
 * Handles: AwaitExprAST
 */

#include "ExprChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkAwaitExpr(AwaitExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkAwaitExpr");
    
    TypeAST* innerType = checkExpr(expr->inner, ctx);
    if (!innerType) return nullptr;
    
    // TODO: Check that innerType is a future type
    // For now, return inner type
    
    return innerType;
}