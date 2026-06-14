/**
 * @file ReturnChecker.cpp
 * @brief Implementation of return statement checking.
 * 
 * Handles: ReturnStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkReturnStmt(ReturnStmtAST* retStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkReturnStmt: " << retStmt->values.size() << " values");
    
    bool isVoidReturn = retStmt->values.empty();
    bool expectsVoid = TypeChecker::isVoid(expectedReturn);
    
    if (isVoidReturn && expectsVoid) {
        // Void function returning nothing – OK
        return;
    }
    
    if (isVoidReturn && !expectsVoid) {
        ctx.error(retStmt->loc, DiagCode::E2001,
                  "non-void function must return a value");
        return;
    }
    
    if (!isVoidReturn && expectsVoid) {
        ctx.error(retStmt->loc, DiagCode::E2001,
                  "void function cannot return a value");
        return;
    }
    
    // Check return value(s) against expected return type(s)
    if (retStmt->values.size() == 1) {
        TypeAST* retType = checkExpr(retStmt->values[0], ctx);
        if (retType && expectedReturn) {
            if (!TypeChecker::isAssignable(retType, expectedReturn, ctx)) {
                ctx.error(retStmt->values[0]->loc, DiagCode::E2001,
                          "return value type does not match function return type");
            }
        }
    } else {
        // Multi-return – need to check against multiple return types
        ctx.error(retStmt->loc, DiagCode::E2001,
                  "multi-value return not yet supported");
    }
}