/**
 * @file ExprStmtChecker.cpp
 * @brief Implementation of expression statement checking.
 * 
 * Handles: ExprStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkExprStmt(ExprStmtAST* exprStmt, SemanticContext& ctx, TypeAST* /*expectedReturn*/) {
    LUC_LOG_SEMANTIC_EXTREME("checkExprStmt");
    
    TypeAST* resultType = checkExpr(exprStmt->expr, ctx);
    
    // Warning for discarded non-void result (except for function calls and assignments)
    if (resultType && !TypeChecker::isVoid(resultType)) {
        // Don't warn for void function calls or assignments
        if (!exprStmt->expr->isa<CallExprAST>() &&
            !exprStmt->expr->isa<AssignExprAST>() &&
            !exprStmt->expr->isa<AwaitExprAST>()) {
            ctx.warning(exprStmt->loc, DiagCode::W6001,
                        "expression result discarded");
        }
    }
}