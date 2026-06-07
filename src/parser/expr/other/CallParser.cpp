/**
 * @file CallParser.cpp
 * @brief Parses function and method call expressions.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements parsing of function calls, method calls, and generic
 * instantiations with argument lists. The `parseCallExpr()` function is called
 * from `parsePostfixExpr()` when a `(` token is encountered after a callable
 * expression.
 * 
 * Grammar (from LUC_GRAMMAR.md):
 *   postfix_op := '(' [ arg_list ] ')'                -- regular call
 *               | generic_args '(' [ arg_list ] ')'   -- generic instantiation
 * 
 * IMPORTANT: The argument pack `!` suffix is NOT allowed in regular calls.
 * It is part of the pipeline step syntax only (see PipelineParser.cpp).
 * 
 * The caller (parsePostfixExpr) handles generic arguments before the `(`.
 * This function only consumes the argument list.
 * 
 * Examples:
 *   f(1, 2, 3)                    → regular call
 *   obj:method(a, b)              → method call (callee is BehaviorAccessExprAST)
 *   arr[idx](x, y)                → array element call (callee is IndexExprAST)
 * 
 * @see ParserExpr.cpp for parsePostfixExpr integration
 * @see ParserHelpers.cpp for parseArgList implementation
 */

#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Call Expression Parser
// ============================================================================

/**
 * @brief Parses a function or method call expression.
 *
 * Grammar:
 *   call_expr := '(' [ arg_list ] ')'
 *
 * The callee expression (function name, method reference, or any expression
 * that yields a function type) is already parsed by the caller. This function
 * consumes the parentheses and argument list, then builds the CallExprAST node.
 *
 * @param callee The expression being called (e.g., identifier, method access).
 * @param genericArgs Optional type arguments for generic instantiation
 *                    (already parsed by the caller).
 * @return ExprPtr – CallExprAST on success, or UnknownExprAST on error.
 *
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at '('
 * On exit:  positioned after the closing ')'
 *
 * ─── Argument Parsing ─────────────────────────────────────────────────────
 *   - If the argument list is not empty, `parseArgList()` parses comma‑separated
 *     expressions until the closing ')'.
 *   - `parseArgList()` uses a consecutive error counter (max 5) to prevent
 *     infinite loops on malformed input.
 *
 * ─── Error Recovery ───────────────────────────────────────────────────────
 *   - Missing '(' after callee: `consume()` reports error and returns a dummy
 *     token; the caller (parsePostfixExpr) handles the error.
 *   - Missing ')': `consume()` reports error; the function still returns a
 *     CallExprAST (with whatever arguments were parsed).
 *   - Unexpected `!` after ')': reports error, skips the `!` (recovery).
 *
 * ─── Semantic Notes (Not Parser Responsibility) ───────────────────────────
 *   - The callee must resolve to a function type (plain, ~async, or generic).
 *   - The number and types of arguments must match the function's parameters.
 *   - If the callee is ~async, the call site must be preceded by `await`.
 *   - If the callee is ~nullable, the call must be guarded by a nil check.
 *   - Generic arguments must match the callee's generic parameters.
 *   - The `!` suffix is NOT allowed here – it is a pipeline‑only feature.
 */
ExprPtr Parser::parseCallExpr(ExprPtr callee, ArenaSpan<TypePtr> genericArgs) {
    LUC_LOG_EXPR_VERBOSE("parseCallExpr: entering at line " << callee->loc.line()
                         << ", col " << callee->loc.column()
                         << ", generic args = " << genericArgs.size());
    
    SourceLocation loc = callee->loc;
    
    ts_.consume(TokenType::LPAREN, "expected '('");

    auto node = arena_.make<CallExprAST>();
    node->loc = loc;
    node->callee = callee;
    node->genericArgs = genericArgs;

    // Parse argument list if not empty
    if (!ts_.check(TokenType::RPAREN)) {
        node->args = parseArgList();
    } else {
        LUC_LOG_EXPR_EXTREME("parseCallExpr: no arguments");
    }

    ts_.consume(TokenType::RPAREN, "expected ')' to close argument list");
    
    // The `!` suffix is NOT allowed in regular function calls.
    // If we see a `!`, it's a syntax error – the user likely meant a pipeline step.
    if (ts_.check(TokenType::BANG)) {
        LUC_LOG_EXPR("parseCallExpr: ERROR - unexpected '!' after argument list");
        errorAt(DiagCode::E1001, 
                "'!' argument pack annotation is only allowed inside pipeline steps (|>). "
                "For a regular function call, remove the '!'. If you intended a pipeline, "
                "use the '|>' operator before this call.");
        ts_.advance(); // consume '!' to recover
    }

    LUC_LOG_EXPR_VERBOSE("parseCallExpr: success");
    return node;
}