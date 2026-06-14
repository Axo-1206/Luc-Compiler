/**
 * @file IfChecker.cpp
 * @brief Implementation of if statement checking.
 * 
 * Handles: IfStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkIfStmt(IfStmtAST* ifStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkIfStmt");
    
    // Check condition
    TypeAST* condType = checkExpr(ifStmt->condition, ctx);
    if (condType) {
        stmt::expectBoolean(condType, ifStmt->condition->loc, ctx);
    }
    
    // Check then branch
    checkStmt(ifStmt->thenBranch, ctx, expectedReturn);
    
    // Check else branch if present
    if (ifStmt->elseBranch) {
        checkStmt(ifStmt->elseBranch, ctx, expectedReturn);
    }
}