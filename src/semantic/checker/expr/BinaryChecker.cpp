/**
 * @file BinaryChecker.cpp
 * @brief Implementation of binary expression checking.
 * 
 * Handles: BinaryExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkBinaryExpr(BinaryExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkBinaryExpr: op=" << static_cast<int>(expr->op));
    
    TypeAST* leftType = checkExpr(expr->left, ctx);
    TypeAST* rightType = checkExpr(expr->right, ctx);
    if (!leftType || !rightType) return nullptr;
    
    leftType = TypeChecker::getUnderlyingType(leftType, *ctx.typeResolver);
    rightType = TypeChecker::getUnderlyingType(rightType, *ctx.typeResolver);
    
    bool isComparison = false;
    bool isLogical = false;
    bool isBitwise = false;
    
    switch (expr->op) {
        case BinaryOp::Eq:
        case BinaryOp::Ne:
        case BinaryOp::Lt:
        case BinaryOp::Gt:
        case BinaryOp::Le:
        case BinaryOp::Ge:
        case BinaryOp::RefEq:
            isComparison = true;
            break;
            
        case BinaryOp::And:
        case BinaryOp::Or:
            isLogical = true;
            break;
            
        case BinaryOp::BitAnd:
        case BinaryOp::BitOr:
        case BinaryOp::BitXor:
        case BinaryOp::Shl:
        case BinaryOp::Shr:
            isBitwise = true;
            break;
            
        default:
            break;
    }
    
    if (isComparison) {
        if (!TypeChecker::typesEqualForComparison(leftType, rightType, *ctx.typeResolver)) {
            ctx.error(expr->loc, DiagCode::E2001, "cannot compare different types");
            return nullptr;
        }
        return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Bool);
    }
    
    if (isLogical) {
        if (!TypeChecker::isBoolean(leftType, *ctx.typeResolver) ||
            !TypeChecker::isBoolean(rightType, *ctx.typeResolver)) {
            ctx.error(expr->loc, DiagCode::E2001, "logical operators require boolean operands");
            return nullptr;
        }
        return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Bool);
    }
    
    if (isBitwise) {
        if (!TypeChecker::isInteger(leftType, *ctx.typeResolver) ||
            !TypeChecker::isInteger(rightType, *ctx.typeResolver)) {
            ctx.error(expr->loc, DiagCode::E2001, "bitwise operators require integer operands");
            return nullptr;
        }
        return TypeChecker::commonType(leftType, rightType, ctx);
    }
    
    // Arithmetic operators
    if (!TypeChecker::isNumeric(leftType, *ctx.typeResolver) ||
        !TypeChecker::isNumeric(rightType, *ctx.typeResolver)) {
        ctx.error(expr->loc, DiagCode::E2001, "arithmetic operators require numeric operands");
        return nullptr;
    }
    
    return TypeChecker::commonType(leftType, rightType, ctx);
}