/**
 * @file MultiVarChecker.cpp
 * @brief Implementation of multi-variable declaration and assignment checking.
 * 
 * Handles: MultiVarDeclAST, MultiAssignStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkMultiVarDecl(MultiVarDeclAST* multiDecl, SemanticContext& ctx, TypeAST* /*expectedReturn*/) {
    LUC_LOG_SEMANTIC_EXTREME("checkMultiVarDecl: " << multiDecl->vars.size() << " variables");
    
    // Check RHS expression
    TypeAST* rhsType = checkExpr(multiDecl->rhs, ctx);
    
    if (rhsType) {
        if (multiDecl->vars.size() == 1) {
            if (!TypeChecker::isAssignable(rhsType, multiDecl->vars[0].second, ctx)) {
                ctx.error(multiDecl->rhs->loc, DiagCode::E2001,
                          "cannot initialize variable with value of different type");
            }
        } else {
            ctx.error(multiDecl->loc, DiagCode::E2001,
                      "multi-value initialization not yet supported");
        }
    }
    
    // Variables are already declared in the scope by the collector
}

void checkMultiAssignStmt(MultiAssignStmtAST* multiAssign, SemanticContext& ctx, TypeAST* /*expectedReturn*/) {
    LUC_LOG_SEMANTIC_EXTREME("checkMultiAssignStmt: " << multiAssign->lhs.size() << " targets");
    
    // Check RHS expression
    TypeAST* rhsType = checkExpr(multiAssign->rhs, ctx);
    
    if (rhsType) {
        if (multiAssign->lhs.size() == 1) {
            TypeAST* lhsType = checkExpr(multiAssign->lhs[0], ctx);
            if (lhsType && !TypeChecker::isAssignable(rhsType, lhsType, ctx)) {
                ctx.error(multiAssign->rhs->loc, DiagCode::E2001,
                          "cannot assign value to left-hand side");
            }
        } else {
            ctx.error(multiAssign->loc, DiagCode::E2001,
                      "multi-value assignment not yet supported");
        }
    }
}