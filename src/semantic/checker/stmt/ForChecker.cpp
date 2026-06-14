/**
 * @file ForChecker.cpp
 * @brief Implementation of for loop checking.
 * 
 * Handles: ForStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkForStmt(ForStmtAST* forStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkForStmt: iterVar=" << ctx.pool.lookup(forStmt->iterVar->name));
    
    // Push scope for the iteration variable
    ctx.scope.push();
    
    // Check iterable expression
    TypeAST* iterableType = checkExpr(forStmt->iterable, ctx);
    if (iterableType) {
        iterableType = TypeChecker::getUnderlyingType(iterableType, *ctx.typeResolver);
        
        // For range loops, iterable should be a range (which we treat as int)
        if (forStmt->iterable->isa<RangeExprAST>()) {
            if (forStmt->iterVar->type) {
                stmt::expectInteger(forStmt->iterVar->type, forStmt->iterVar->loc, ctx);
            }
        } else {
            // Collection iteration
            if (!TypeChecker::isArray(iterableType, *ctx.typeResolver)) {
                ctx.error(forStmt->iterable->loc, DiagCode::E2001,
                          "for loop iterable must be an array or range");
            }
            
            // Check that iterVar type matches element type
            TypeAST* elemType = TypeChecker::getElementType(iterableType, *ctx.typeResolver);
            if (elemType && forStmt->iterVar->type) {
                if (!TypeChecker::isAssignable(elemType, forStmt->iterVar->type, ctx)) {
                    ctx.error(forStmt->iterVar->loc, DiagCode::E2001,
                              "iteration variable type does not match array element type");
                }
            }
        }
    }
    
    // Check step expression if present
    if (forStmt->step) {
        TypeAST* stepType = checkExpr(forStmt->step, ctx);
        if (stepType) {
            stmt::expectInteger(stepType, forStmt->step->loc, ctx);
        }
    }
    
    // Check loop body (with loop depth tracking)
    ctx.enterLoop();
    checkStmt(forStmt->body, ctx, expectedReturn);
    ctx.exitLoop();
    
    ctx.scope.pop();
}