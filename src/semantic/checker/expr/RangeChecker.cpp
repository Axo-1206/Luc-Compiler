/**
 * @file RangeChecker.cpp
 * @brief Implementation of range expression checking.
 * 
 * Handles: RangeExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkRangeExpr(RangeExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkRangeExpr");
    
    TypeAST* loType = checkExpr(expr->lo, ctx);
    TypeAST* hiType = checkExpr(expr->hi, ctx);
    if (!loType || !hiType) return nullptr;
    
    loType = TypeChecker::getUnderlyingType(loType, *ctx.typeResolver);
    hiType = TypeChecker::getUnderlyingType(hiType, *ctx.typeResolver);
    
    if (!TypeChecker::isInteger(loType, *ctx.typeResolver) ||
        !TypeChecker::isInteger(hiType, *ctx.typeResolver)) {
        ctx.error(expr->loc, DiagCode::E2001, "range bounds must be integers");
        return nullptr;
    }
    
    return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Int);
}