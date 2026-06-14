/**
 * @file StmtChecker.hpp
 * @brief Statement semantic validation and control flow checking - Main entry point.
 * 
 * ============================================================================
 * STATEMENT CHECKER MODULE
 * ============================================================================
 * 
 * This module validates statements and their control flow semantics.
 * 
 * ─── Architecture ──────────────────────────────────────────────────────────
 * 
 *   The module follows a dispatch pattern:
 *   
 *   checkStmt() ─┬─► checkBlockStmt()       (BlockChecker.cpp)
 *                ├─► checkDeclStmt()         (DeclStmtChecker.cpp)
 *                ├─► checkExprStmt()         (ExprStmtChecker.cpp)
 *                ├─► checkIfStmt()           (IfChecker.cpp)
 *                ├─► checkSwitchStmt()       (SwitchChecker.cpp)
 *                ├─► checkForStmt()          (ForChecker.cpp)
 *                ├─► checkWhileStmt()        (WhileChecker.cpp)
 *                ├─► checkDoWhileStmt()      (WhileChecker.cpp)
 *                ├─► checkReturnStmt()       (ReturnChecker.cpp)
 *                ├─► checkBreakStmt()        (JumpChecker.cpp)
 *                ├─► checkContinueStmt()     (JumpChecker.cpp)
 *                ├─► checkMultiVarDecl()     (MultiChecker.cpp)
 *                └─► checkMultiAssignStmt()  (MultiChecker.cpp)
 * 
 * ─── Dependencies ──────────────────────────────────────────────────────────
 * 
 *   - ExprChecker: for expression validation
 *   - TypeChecker: for type compatibility
 *   - ScopeStack: for scope management
 * 
 * ─── Control Flow Context ─────────────────────────────────────────────────
 * 
 *   - ctx.loopDepth: tracks nesting for break/continue validation
 *   - expectedReturn: the function's return type (for return statements)
 * 
 * @see ExprChecker for expression validation
 * @see TypeChecker for type utilities
 */

#pragma once

#include "ast/StmtAST.hpp"
#include "ast/ExprAST.hpp"
#include "ast/DeclAST.hpp"
#include "semantic/helpers/SemanticContext.hpp"
#include "semantic/scope/ScopeStack.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "semantic/checker/expr/ExprChecker.hpp"

// ============================================================================
// Main Dispatch
// ============================================================================

/**
 * @brief Main entry point for statement checking.
 * 
 * Dispatches to the appropriate checker based on statement kind.
 * 
 * @param stmt The statement to check
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type (for return statements)
 */
void checkStmt(StmtAST* stmt, SemanticContext& ctx, TypeAST* expectedReturn = nullptr);

// ============================================================================
// Block Statements
// ============================================================================

/**
 * @brief Checks a block statement.
 * 
 * Pushes a new scope, checks all statements in order, then pops the scope.
 * 
 * @param block The block statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkBlockStmt(BlockStmtAST* block, SemanticContext& ctx, TypeAST* expectedReturn);

// ============================================================================
// Declaration Statements
// ============================================================================

/**
 * @brief Checks a declaration statement (local var/func).
 * 
 * @param declStmt The declaration statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type (unused)
 */
void checkDeclStmt(DeclStmtAST* declStmt, SemanticContext& ctx, TypeAST* expectedReturn = nullptr);

// ============================================================================
// Expression Statements
// ============================================================================

/**
 * @brief Checks an expression statement (expression with discarded value).
 * 
 * @param exprStmt The expression statement
 * @param ctx Semantic context
 * @param expectedReturn Unused
 */
void checkExprStmt(ExprStmtAST* exprStmt, SemanticContext& ctx, TypeAST* expectedReturn = nullptr);

// ============================================================================
// Branching Statements
// ============================================================================

/**
 * @brief Checks an if statement.
 * 
 * Condition must be boolean. Then/else branches are checked.
 * 
 * @param ifStmt The if statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkIfStmt(IfStmtAST* ifStmt, SemanticContext& ctx, TypeAST* expectedReturn);

/**
 * @brief Checks a switch statement.
 * 
 * Subject must be of a type that supports equality comparison.
 * Case values must be constant and of the same type as the subject.
 * 
 * @param switchStmt The switch statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkSwitchStmt(SwitchStmtAST* switchStmt, SemanticContext& ctx, TypeAST* expectedReturn);

// ============================================================================
// Loop Statements
// ============================================================================

/**
 * @brief Checks a for loop statement.
 * 
 * @param forStmt The for statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkForStmt(ForStmtAST* forStmt, SemanticContext& ctx, TypeAST* expectedReturn);

/**
 * @brief Checks a while loop statement.
 * 
 * Condition must be boolean.
 * 
 * @param whileStmt The while statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkWhileStmt(WhileStmtAST* whileStmt, SemanticContext& ctx, TypeAST* expectedReturn);

/**
 * @brief Checks a do-while loop statement.
 * 
 * Body executes at least once; condition checked after.
 * 
 * @param doWhileStmt The do-while statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type
 */
void checkDoWhileStmt(DoWhileStmtAST* doWhileStmt, SemanticContext& ctx, TypeAST* expectedReturn);

// ============================================================================
// Jump Statements
// ============================================================================

/**
 * @brief Checks a return statement.
 * 
 * Values must match the function's expected return type.
 * 
 * @param retStmt The return statement
 * @param ctx Semantic context
 * @param expectedReturn The function's expected return type (may be nullptr for void)
 */
void checkReturnStmt(ReturnStmtAST* retStmt, SemanticContext& ctx, TypeAST* expectedReturn);

/**
 * @brief Checks a break statement.
 * 
 * Must be inside a loop (loopDepth > 0).
 * 
 * @param breakStmt The break statement
 * @param ctx Semantic context
 */
void checkBreakStmt(BreakStmtAST* breakStmt, SemanticContext& ctx);

/**
 * @brief Checks a continue statement.
 * 
 * Must be inside a loop (loopDepth > 0).
 * 
 * @param continueStmt The continue statement
 * @param ctx Semantic context
 */
void checkContinueStmt(ContinueStmtAST* continueStmt, SemanticContext& ctx);

// ============================================================================
// Multi-Variable Declarations and Assignments
// ============================================================================

/**
 * @brief Checks a multi-variable declaration (let x int, y int = f()).
 * 
 * @param multiDecl The multi-variable declaration
 * @param ctx Semantic context
 * @param expectedReturn Unused
 */
void checkMultiVarDecl(MultiVarDeclAST* multiDecl, SemanticContext& ctx, TypeAST* expectedReturn = nullptr);

/**
 * @brief Checks a multi-assignment statement (x, y = f()).
 * 
 * @param multiAssign The multi-assignment statement
 * @param ctx Semantic context
 * @param expectedReturn Unused
 */
void checkMultiAssignStmt(MultiAssignStmtAST* multiAssign, SemanticContext& ctx, TypeAST* expectedReturn = nullptr);

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @namespace stmt
 * @brief Internal utilities for statement checking (not part of public API).
 * 
 * These helpers are used by the individual statement checkers but are not
 * intended for external use.
 */
namespace stmt {

/**
 * @brief Checks if a type is boolean, emitting an error if not.
 * 
 * @param type The type to check
 * @param loc Source location for error reporting
 * @param ctx Semantic context
 * @return true if type is boolean
 */
bool expectBoolean(TypeAST* type, const SourceLocation& loc, SemanticContext& ctx);

/**
 * @brief Checks if a type is integer, emitting an error if not.
 * 
 * @param type The type to check
 * @param loc Source location for error reporting
 * @param ctx Semantic context
 * @return true if type is integer
 */
bool expectInteger(TypeAST* type, const SourceLocation& loc, SemanticContext& ctx);

/**
 * @brief Checks if an expression is constant, emitting an error if not.
 * 
 * @param expr The expression to check
 * @param ctx Semantic context
 * @return true if expression is constant
 */
bool expectConstant(ExprAST* expr, SemanticContext& ctx);

} // namespace stmt