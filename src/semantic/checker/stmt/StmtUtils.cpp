/**
 * @file StmtUtils.cpp
 * @brief Implementation of shared utility functions for statement checking.
 */

#include "StmtChecker.hpp"

namespace stmt {

bool expectBoolean(TypeAST* type, const SourceLocation& loc, SemanticContext& ctx) {
    if (!type) return false;
    
    type = TypeChecker::getUnderlyingType(type, *ctx.typeResolver);
    
    if (!TypeChecker::isBoolean(type, *ctx.typeResolver)) {
        ctx.error(loc, DiagCode::E2001, "expected boolean type");
        return false;
    }
    return true;
}

bool expectInteger(TypeAST* type, const SourceLocation& loc, SemanticContext& ctx) {
    if (!type) return false;
    
    type = TypeChecker::getUnderlyingType(type, *ctx.typeResolver);
    
    if (!TypeChecker::isInteger(type, *ctx.typeResolver)) {
        ctx.error(loc, DiagCode::E2001, "expected integer type");
        return false;
    }
    return true;
}

bool expectConstant(ExprAST* expr, SemanticContext& ctx) {
    if (!expr) return false;
    
    if (!expr->isConst) {
        ctx.error(expr->loc, DiagCode::E2001, "expected constant expression");
        return false;
    }
    return true;
}

} // namespace stmt