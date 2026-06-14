/**
 * @file ExprChecker.hpp
 * @brief Expression type checking and semantic validation - Main entry point.
 * 
 * ============================================================================
 * EXPRESSION CHECKER MODULE
 * ============================================================================
 * 
 * This module validates expressions and computes their resolved types.
 * The resolved type is cached on ExprAST::resolvedType.
 * 
 * ─── Architecture ──────────────────────────────────────────────────────────
 * 
 *   The module follows a dispatch pattern:
 *   
 *   checkExpr() ─┬─► checkLiteralExpr()       (LiteralChecker.cpp)
 *                ├─► checkArrayLiteralExpr()  (LiteralChecker.cpp)
 *                ├─► checkStructLiteralExpr() (LiteralChecker.cpp)
 *                ├─► checkIdentifierExpr()    (IdentifierChecker.cpp)
 *                ├─► checkFieldAccessExpr()   (FieldAccessChecker.cpp)
 *                ├─► checkBehaviorAccessExpr()(FieldAccessChecker.cpp)
 *                ├─► checkCallExpr()          (CallChecker.cpp)
 *                ├─► checkIndexExpr()         (IndexSliceChecker.cpp)
 *                ├─► checkSliceExpr()         (IndexSliceChecker.cpp)
 *                ├─► checkUnaryExpr()         (UnaryChecker.cpp)
 *                ├─► checkBinaryExpr()        (BinaryChecker.cpp)
 *                ├─► checkAssignExpr()        (AssignChecker.cpp)
 *                ├─► checkNullCoalesceExpr()  (NullableChecker.cpp)
 *                ├─► checkNullableChainExpr() (NullableChecker.cpp)
 *                ├─► checkIsExpr()            (TypeTestChecker.cpp)
 *                ├─► checkAnonFuncExpr()      (FunctionChecker.cpp)
 *                ├─► checkAwaitExpr()         (AwaitChecker.cpp)
 *                └─► checkRangeExpr()         (RangeChecker.cpp)
 * 
 * ─── Dependencies ──────────────────────────────────────────────────────────
 * 
 *   - TypeResolver: for type resolution and alias unwrapping
 *   - TypeChecker: for type compatibility and inference
 *   - ScopeStack: for name lookup (identifiers)
 * 
 * @see TypeResolver for type resolution
 * @see TypeChecker for type utilities
 */

#pragma once

#include "ast/ExprAST.hpp"
#include "ast/TypeAST.hpp"
#include "semantic/helpers/SemanticContext.hpp"
#include "semantic/scope/ScopeStack.hpp"
#include "semantic/resolver/TypeResolver.hpp"
#include "semantic/checker/TypeChecker.hpp"

// ============================================================================
// Main Dispatch
// ============================================================================

/**
 * @brief Main entry point for expression checking.
 * 
 * Dispatches to the appropriate checker based on expression kind.
 * Caches the resolved type on expr->resolvedType.
 * 
 * @param expr The expression to check
 * @param ctx Semantic context
 * @return TypeAST* The resolved type, or nullptr on error
 */
TypeAST* checkExpr(ExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Literal Expressions
// ============================================================================

/**
 * @brief Checks a literal expression (integer, float, string, bool, nil).
 * 
 * @param expr The literal expression
 * @param ctx Semantic context
 * @return TypeAST* The literal's type
 */
TypeAST* checkLiteralExpr(LiteralExprAST* expr, SemanticContext& ctx);

/**
 * @brief Checks an array literal expression.
 * 
 * @param expr The array literal
 * @param ctx Semantic context
 * @return TypeAST* The array type (inferred from elements)
 */
TypeAST* checkArrayLiteralExpr(ArrayLiteralExprAST* expr, SemanticContext& ctx);

/**
 * @brief Checks a struct literal expression.
 * 
 * @param expr The struct literal
 * @param ctx Semantic context
 * @return TypeAST* The struct type
 */
TypeAST* checkStructLiteralExpr(StructLiteralExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Identifier Expressions
// ============================================================================

/**
 * @brief Checks an identifier expression (variable, function, type reference).
 * 
 * Resolves the name in the value namespace first, then type namespace.
 * 
 * @param expr The identifier expression
 * @param ctx Semantic context
 * @return TypeAST* The resolved type
 */
TypeAST* checkIdentifierExpr(IdentifierExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Field and Method Access
// ============================================================================

/**
 * @brief Checks a field access expression (obj.field).
 * 
 * @param expr The field access expression
 * @param ctx Semantic context
 * @return TypeAST* The field's type
 */
TypeAST* checkFieldAccessExpr(FieldAccessExprAST* expr, SemanticContext& ctx);

/**
 * @brief Checks a behavior access expression (obj:method).
 * 
 * This is a method REFERENCE, not a call. The result is a function type.
 * 
 * @param expr The behavior access expression
 * @param ctx Semantic context
 * @return TypeAST* The method's function type
 */
TypeAST* checkBehaviorAccessExpr(BehaviorAccessExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Call Expressions
// ============================================================================

/**
 * @brief Checks a function or method call expression.
 * 
 * @param expr The call expression
 * @param ctx Semantic context
 * @return TypeAST* The return type of the called function
 */
TypeAST* checkCallExpr(CallExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Index and Slice Expressions
// ============================================================================

/**
 * @brief Checks an array index expression (arr[index]).
 * 
 * @param expr The index expression
 * @param ctx Semantic context
 * @return TypeAST* The element type
 */
TypeAST* checkIndexExpr(IndexExprAST* expr, SemanticContext& ctx);

/**
 * @brief Checks an array slice expression (arr[start..end]).
 * 
 * @param expr The slice expression
 * @param ctx Semantic context
 * @return TypeAST* The slice type ([_, T])
 */
TypeAST* checkSliceExpr(SliceExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Unary Expressions
// ============================================================================

/**
 * @brief Checks a unary expression (-x, not x, ~~x, &x).
 * 
 * @param expr The unary expression
 * @param ctx Semantic context
 * @return TypeAST* The result type
 */
TypeAST* checkUnaryExpr(UnaryExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Binary Expressions
// ============================================================================

/**
 * @brief Checks a binary expression (a + b, a == b, etc.).
 * 
 * @param expr The binary expression
 * @param ctx Semantic context
 * @return TypeAST* The result type
 */
TypeAST* checkBinaryExpr(BinaryExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Assignment Expressions
// ============================================================================

/**
 * @brief Checks an assignment expression (lhs = rhs).
 * 
 * @param expr The assignment expression
 * @param ctx Semantic context
 * @return TypeAST* The type of the assignment (type of lhs)
 */
TypeAST* checkAssignExpr(AssignExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Nullable Expressions
// ============================================================================

/**
 * @brief Checks a null coalesce expression (value ?? fallback).
 * 
 * @param expr The null coalesce expression
 * @param ctx Semantic context
 * @return TypeAST* The unwrapped type (non-nullable)
 */
TypeAST* checkNullCoalesceExpr(NullCoalesceExprAST* expr, SemanticContext& ctx);

/**
 * @brief Checks a nullable chain expression (obj?.field?.field).
 * 
 * @param expr The nullable chain expression
 * @param ctx Semantic context
 * @return TypeAST* The type of the chain (nullable)
 */
TypeAST* checkNullableChainExpr(NullableChainExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Type Test Expression
// ============================================================================

/**
 * @brief Checks a type test expression (x is Type).
 * 
 * @param expr The is expression
 * @param ctx Semantic context
 * @return TypeAST* The bool type
 */
TypeAST* checkIsExpr(IsExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Function Expressions
// ============================================================================

/**
 * @brief Checks an anonymous function expression.
 * 
 * @param expr The anonymous function
 * @param ctx Semantic context
 * @return TypeAST* The function type
 */
TypeAST* checkAnonFuncExpr(AnonFuncExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Await Expression
// ============================================================================

/**
 * @brief Checks an await expression (await future).
 * 
 * @param expr The await expression
 * @param ctx Semantic context
 * @return TypeAST* The future's value type
 */
TypeAST* checkAwaitExpr(AwaitExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Range Expression
// ============================================================================

/**
 * @brief Checks a range expression (lo..hi).
 * 
 * @param expr The range expression
 * @param ctx Semantic context
 * @return TypeAST* The range type
 */
TypeAST* checkRangeExpr(RangeExprAST* expr, SemanticContext& ctx);

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @namespace expr
 * @brief Internal utilities for expression checking (not part of public API).
 * 
 * These helpers are used by the individual expression checkers but are not
 * intended for external use.
 */
namespace expr {

/**
 * @brief Ensures a type is resolved and unwraps aliases.
 * 
 * @param type The type to resolve
 * @param ctx Semantic context
 * @return TypeAST* The resolved type, or nullptr on error
 */
TypeAST* resolveType(TypeAST* type, SemanticContext& ctx);

/**
 * @brief Checks if a type is valid for numeric operations.
 * 
 * @param type The type to check
 * @param ctx Semantic context
 * @return true if type is numeric
 */
bool isNumericType(TypeAST* type, SemanticContext& ctx);

/**
 * @brief Checks if a type is valid for integer operations.
 * 
 * @param type The type to check
 * @param ctx Semantic context
 * @return true if type is integer
 */
bool isIntegerType(TypeAST* type, SemanticContext& ctx);

/**
 * @brief Checks if a type is valid for boolean operations.
 * 
 * @param type The type to check
 * @param ctx Semantic context
 * @return true if type is boolean
 */
bool isBooleanType(TypeAST* type, SemanticContext& ctx);

/**
 * @brief Gets the element type from an array type.
 * 
 * @param arrayType The array type
 * @param ctx Semantic context
 * @return TypeAST* The element type, or nullptr
 */
TypeAST* getArrayElementType(TypeAST* arrayType, SemanticContext& ctx);

} // namespace expr