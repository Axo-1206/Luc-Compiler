/**
 * @file ExprChecker.cpp
 * @brief Implementation of the expression dispatcher.
 * 
 * This file contains only the dispatch logic. All expression-specific
 * validation is implemented in their respective .cpp files.
 */

#include "ExprChecker.hpp"
#include "debug/DebugMacros.hpp"
#include "debug/DebugUtils.hpp"

TypeAST* checkExpr(ExprAST* expr, SemanticContext& ctx) {
    if (!expr) return nullptr;
    
    // Return cached result if already computed
    if (expr->resolvedType) {
        return expr->resolvedType;
    }
    
    LUC_LOG_SEMANTIC_EXTREME("checkExpr: kind=" << LucDebug::kindToString(expr->kind));
    
    TypeAST* result = nullptr;
    
    switch (expr->kind) {
        case ASTKind::LiteralExpr:
            result = checkLiteralExpr(expr->as<LiteralExprAST>(), ctx);
            break;
        case ASTKind::ArrayLiteralExpr:
            result = checkArrayLiteralExpr(expr->as<ArrayLiteralExprAST>(), ctx);
            break;
        case ASTKind::StructLiteralExpr:
            result = checkStructLiteralExpr(expr->as<StructLiteralExprAST>(), ctx);
            break;
        case ASTKind::IdentifierExpr:
            result = checkIdentifierExpr(expr->as<IdentifierExprAST>(), ctx);
            break;
        case ASTKind::FieldAccessExpr:
            result = checkFieldAccessExpr(expr->as<FieldAccessExprAST>(), ctx);
            break;
        case ASTKind::BehaviorAccessExpr:
            result = checkBehaviorAccessExpr(expr->as<BehaviorAccessExprAST>(), ctx);
            break;
        case ASTKind::CallExpr:
            result = checkCallExpr(expr->as<CallExprAST>(), ctx);
            break;
        case ASTKind::IndexExpr:
            result = checkIndexExpr(expr->as<IndexExprAST>(), ctx);
            break;
        case ASTKind::SliceExpr:
            result = checkSliceExpr(expr->as<SliceExprAST>(), ctx);
            break;
        case ASTKind::UnaryExpr:
            result = checkUnaryExpr(expr->as<UnaryExprAST>(), ctx);
            break;
        case ASTKind::BinaryExpr:
            result = checkBinaryExpr(expr->as<BinaryExprAST>(), ctx);
            break;
        case ASTKind::AssignExpr:
            result = checkAssignExpr(expr->as<AssignExprAST>(), ctx);
            break;
        case ASTKind::IsExpr:
            result = checkIsExpr(expr->as<IsExprAST>(), ctx);
            break;
        case ASTKind::NullCoalesceExpr:
            result = checkNullCoalesceExpr(expr->as<NullCoalesceExprAST>(), ctx);
            break;
        case ASTKind::NullableChainExpr:
            result = checkNullableChainExpr(expr->as<NullableChainExprAST>(), ctx);
            break;
        case ASTKind::AnonFuncExpr:
            result = checkAnonFuncExpr(expr->as<AnonFuncExprAST>(), ctx);
            break;
        case ASTKind::AwaitExpr:
            result = checkAwaitExpr(expr->as<AwaitExprAST>(), ctx);
            break;
        case ASTKind::RangeExpr:
            result = checkRangeExpr(expr->as<RangeExprAST>(), ctx);
            break;
        default:
            LUC_LOG_SEMANTIC("checkExpr: unhandled expression kind: " 
                             << LucDebug::kindToString(expr->kind));
            break;
    }
    
    // Cache the result
    expr->resolvedType = result;
    return result;
}