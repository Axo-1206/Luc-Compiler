/**
 * @file BlockChecker.cpp
 * @brief Implementation of block statement checking.
 * 
 * Handles: BlockStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkBlockStmt(BlockStmtAST* block, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkBlockStmt: " << block->stmts.size() << " statements");
    
    // Push a new scope for the block
    ctx.scope.push();
    
    // Check all statements in order
    for (auto* stmt : block->stmts) {
        checkStmt(stmt, ctx, expectedReturn);
    }
    
    // Pop the scope
    ctx.scope.pop();
}