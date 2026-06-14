/**
 * @file CallChecker.cpp
 * @brief Implementation of function/method call checking.
 * 
 * Handles: CallExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkCallExpr(CallExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkCallExpr");
    
    TypeAST* calleeType = checkExpr(expr->callee, ctx);
    if (!calleeType) return nullptr;
    
    if (!TypeChecker::isCallable(calleeType, *ctx.typeResolver)) {
        ctx.error(expr->callee->loc, DiagCode::E2001, "expression is not callable");
        return nullptr;
    }
    
    std::vector<TypeAST*> paramTypes = TypeChecker::getParameterTypes(calleeType, *ctx.typeResolver);
    
    if (expr->args.size() != paramTypes.size()) {
        ctx.error(expr->loc, DiagCode::E2001,
                  "expected ", paramTypes.size(), " arguments, got ", expr->args.size());
        return nullptr;
    }
    
    for (size_t i = 0; i < expr->args.size(); ++i) {
        TypeAST* argType = checkExpr(expr->args[i], ctx);
        if (!argType) return nullptr;
        
        if (!TypeChecker::isAssignable(argType, paramTypes[i], ctx)) {
            ctx.error(expr->args[i]->loc, DiagCode::E2001,
                      "argument ", i + 1, " has wrong type");
            return nullptr;
        }
    }
    
    return TypeChecker::getReturnType(calleeType, *ctx.typeResolver);
}