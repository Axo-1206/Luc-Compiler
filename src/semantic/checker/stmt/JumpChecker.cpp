/**
 * @file JumpChecker.cpp
 * @brief Implementation of break and continue statement checking.
 * 
 * Handles: BreakStmtAST, ContinueStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkBreakStmt(BreakStmtAST* breakStmt, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkBreakStmt");
    
    if (ctx.loopDepth == 0) {
        ctx.error(breakStmt->loc, DiagCode::E2001,
                  "'break' statement outside of loop");
    }
}

void checkContinueStmt(ContinueStmtAST* continueStmt, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkContinueStmt");
    
    if (ctx.loopDepth == 0) {
        ctx.error(continueStmt->loc, DiagCode::E2001,
                  "'continue' statement outside of loop");
    }
}