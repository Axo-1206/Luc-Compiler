/**
 * @file ExprUtils.cpp
 * @brief Implementation of shared utility functions for expression checking.
 */

#include "ExprChecker.hpp"

namespace expr {

TypeAST* resolveType(TypeAST* type, SemanticContext& ctx) {
    if (!type) return nullptr;
    return ctx.typeResolver->resolve(type);
}

bool isNumericType(TypeAST* type, SemanticContext& ctx) {
    if (!type) return false;
    type = ctx.typeResolver->unwrapAlias(type);
    return TypeChecker::isNumeric(type, *ctx.typeResolver);
}

bool isIntegerType(TypeAST* type, SemanticContext& ctx) {
    if (!type) return false;
    type = ctx.typeResolver->unwrapAlias(type);
    return TypeChecker::isInteger(type, *ctx.typeResolver);
}

bool isBooleanType(TypeAST* type, SemanticContext& ctx) {
    if (!type) return false;
    type = ctx.typeResolver->unwrapAlias(type);
    return TypeChecker::isBoolean(type, *ctx.typeResolver);
}

TypeAST* getArrayElementType(TypeAST* arrayType, SemanticContext& ctx) {
    if (!arrayType) return nullptr;
    arrayType = ctx.typeResolver->unwrapAlias(arrayType);
    return TypeChecker::getElementType(arrayType, *ctx.typeResolver);
}

} // namespace expr