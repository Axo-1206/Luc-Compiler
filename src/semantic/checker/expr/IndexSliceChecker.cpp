/**
 * @file IndexSliceChecker.cpp
 * @brief Implementation of index and slice expression checking.
 * 
 * Handles: IndexExprAST, SliceExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkIndexExpr(IndexExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkIndexExpr");
    
    TypeAST* targetType = checkExpr(expr->target, ctx);
    if (!targetType) return nullptr;
    
    targetType = TypeChecker::getUnderlyingType(targetType, *ctx.typeResolver);
    
    if (!TypeChecker::isArray(targetType, *ctx.typeResolver)) {
        ctx.error(expr->target->loc, DiagCode::E2001, "cannot index non-array type");
        return nullptr;
    }
    
    TypeAST* indexType = checkExpr(expr->index, ctx);
    if (!indexType) return nullptr;
    
    if (!TypeChecker::isInteger(indexType, *ctx.typeResolver)) {
        ctx.error(expr->index->loc, DiagCode::E2001, "array index must be integer");
        return nullptr;
    }
    
    return TypeChecker::getElementType(targetType, *ctx.typeResolver);
}

TypeAST* checkSliceExpr(SliceExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkSliceExpr");
    
    TypeAST* targetType = checkExpr(expr->target, ctx);
    if (!targetType) return nullptr;
    
    targetType = TypeChecker::getUnderlyingType(targetType, *ctx.typeResolver);
    
    if (!TypeChecker::isSliceable(targetType, *ctx.typeResolver)) {
        ctx.error(expr->target->loc, DiagCode::E2001, "cannot slice non-array type");
        return nullptr;
    }
    
    if (expr->start) {
        TypeAST* startType = checkExpr(expr->start, ctx);
        if (!startType) return nullptr;
        
        if (!TypeChecker::isInteger(startType, *ctx.typeResolver)) {
            ctx.error(expr->start->loc, DiagCode::E2001, "slice start must be integer");
            return nullptr;
        }
    }
    
    if (expr->end) {
        TypeAST* endType = checkExpr(expr->end, ctx);
        if (!endType) return nullptr;
        
        if (!TypeChecker::isInteger(endType, *ctx.typeResolver)) {
            ctx.error(expr->end->loc, DiagCode::E2001, "slice end must be integer");
            return nullptr;
        }
    }
    
    TypeAST* elemType = TypeChecker::getElementType(targetType, *ctx.typeResolver);
    TypeAST* sliceType = ctx.arena.make<ArrayTypeAST>(ArrayKind::Slice, 0, elemType);
    
    expr->sliceType = sliceType;
    return sliceType;
}