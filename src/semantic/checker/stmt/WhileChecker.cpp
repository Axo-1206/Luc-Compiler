/**
 * @file WhileChecker.cpp
 * @brief Implementation of while and do-while loop checking.
 * 
 * Handles: WhileStmtAST, DoWhileStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkWhileStmt(WhileStmtAST* whileStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkWhileStmt");
    
    // Check condition
    TypeAST* condType = checkExpr(whileStmt->condition, ctx);
    if (condType) {
        stmt::expectBoolean(condType, whileStmt->condition->loc, ctx);
    }
    
    // Check loop body (with loop depth tracking)
    ctx.enterLoop();
    checkStmt(whileStmt->body, ctx, expectedReturn);
    ctx.exitLoop();
}

void checkDoWhileStmt(DoWhileStmtAST* doWhileStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkDoWhileStmt");
    
    // Check loop body (with loop depth tracking)
    ctx.enterLoop();
    checkStmt(doWhileStmt->body, ctx, expectedReturn);
    ctx.exitLoop();
    
    // Check condition after body
    TypeAST* condType = checkExpr(doWhileStmt->condition, ctx);
    if (condType) {
        stmt::expectBoolean(condType, doWhileStmt->condition->loc, ctx);
    }
}