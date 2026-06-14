/**
 * @file SwitchChecker.cpp
 * @brief Implementation of switch statement checking.
 * 
 * Handles: SwitchStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkSwitchStmt(SwitchStmtAST* switchStmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    LUC_LOG_SEMANTIC_EXTREME("checkSwitchStmt: " << switchStmt->cases.size() << " cases");
    
    // Check subject
    TypeAST* subjectType = checkExpr(switchStmt->subject, ctx);
    if (!subjectType) return;
    
    subjectType = TypeChecker::getUnderlyingType(subjectType, *ctx.typeResolver);
    
    // Check each case
    for (auto* caseClause : switchStmt->cases) {
        // Check case values
        for (auto* value : caseClause->values) {
            TypeAST* valueType = checkExpr(value, ctx);
            if (valueType) {
                valueType = TypeChecker::getUnderlyingType(valueType, *ctx.typeResolver);
                if (!TypeChecker::isEqual(subjectType, valueType, *ctx.typeResolver)) {
                    ctx.error(value->loc, DiagCode::E2001,
                              "case value type does not match switch subject");
                }
                
                // Check if value is constant
                stmt::expectConstant(value, ctx);
            }
        }
        
        // Check case body
        checkStmt(caseClause->body, ctx, expectedReturn);
    }
    
    // Check default body if present
    if (switchStmt->defaultBody) {
        checkStmt(switchStmt->defaultBody, ctx, expectedReturn);
    }
}