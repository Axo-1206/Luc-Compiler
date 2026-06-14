/**
 * @file AssignChecker.cpp
 * @brief Implementation of assignment expression checking.
 * 
 * Handles: AssignExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkAssignExpr(AssignExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkAssignExpr");
    
    TypeAST* rhsType = checkExpr(expr->rhs, ctx);
    if (!rhsType) return nullptr;
    
    TypeAST* lhsType = checkExpr(expr->lhs, ctx);
    if (!lhsType) return nullptr;
    
    if (!TypeChecker::isAssignable(rhsType, lhsType, ctx)) {
        ctx.error(expr->loc, DiagCode::E2001, "cannot assign value to left-hand side");
        return nullptr;
    }
    
    if (expr->op != AssignOp::Assign) {
        if (!TypeChecker::isNumeric(lhsType, *ctx.typeResolver)) {
            ctx.error(expr->loc, DiagCode::E2001, "compound assignment requires numeric type");
            return nullptr;
        }
    }
    
    return lhsType;
}