/**
 * @file UnaryChecker.cpp
 * @brief Implementation of unary expression checking.
 * 
 * Handles: UnaryExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkUnaryExpr(UnaryExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkUnaryExpr: op=" << static_cast<int>(expr->op));
    
    TypeAST* operandType = checkExpr(expr->operand, ctx);
    if (!operandType) return nullptr;
    
    operandType = TypeChecker::getUnderlyingType(operandType, *ctx.typeResolver);
    
    switch (expr->op) {
        case UnaryOp::Neg:
            if (!TypeChecker::isNumeric(operandType, *ctx.typeResolver)) {
                ctx.error(expr->operand->loc, DiagCode::E2001, "negation requires numeric operand");
                return nullptr;
            }
            return operandType;
            
        case UnaryOp::Not:
            if (!TypeChecker::isBoolean(operandType, *ctx.typeResolver)) {
                ctx.error(expr->operand->loc, DiagCode::E2001, "logical not requires boolean operand");
                return nullptr;
            }
            return operandType;
            
        case UnaryOp::BitNot:
            if (!TypeChecker::isInteger(operandType, *ctx.typeResolver)) {
                ctx.error(expr->operand->loc, DiagCode::E2001, "bitwise not requires integer operand");
                return nullptr;
            }
            return operandType;
            
        case UnaryOp::Ref:
            return ctx.arena.make<RefTypeAST>(operandType);
            
        default:
            ctx.error(expr->loc, DiagCode::E2001, "unknown unary operator");
            return nullptr;
    }
}