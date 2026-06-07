/**
 * @file SliceParser.cpp
 * @brief Parses array slice expressions.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements parsing of array slice operations.
 * The `parseSliceExpr()` function is called from `parsePostfixExpr()` when a
 * `[` token is encountered followed by a range pattern (start .. end).
 * 
 * Grammar (from LUC_GRAMMAR.md):
 *   postfix_op := '[' expr '..' expr ']'          -- inclusive slice
 *               | '[' expr '..<' expr ']'         -- exclusive slice
 * 
 * Examples:
 *   nums[1..3]                 → inclusive slice (elements 1, 2, 3)
 *   nums[1..<3]                → exclusive slice (elements 1, 2)
 *   matrix[0..<rows][0..<cols] → nested slice
 * 
 * @see IndexParser.cpp for element access (single index)
 * @see ParserExpr.cpp for parsePostfixExpr integration
 * @see SliceExprAST in ExprAST.hpp for AST representation
 */

#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Slice Expression Parser
// ============================================================================

/**
 * @brief Parses an array slice expression.
 *
 * Grammar:
 *   slice_expr := '[' start_expr ( '..' | '..<' ) end_expr ']'
 *
 * This function consumes a bracketed range expression and produces a
 * SliceExprAST node.
 *
 * @param target The array‑typed expression being sliced (already parsed).
 * @return ExprPtr – SliceExprAST on success, or UnknownExprAST on error.
 *
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at '['
 * On exit:  positioned after the closing ']'
 *
 * ─── Slice Syntax ─────────────────────────────────────────────────────────
 *   - Inclusive slice: `[ start .. end ]` – both ends inclusive
 *   - Exclusive slice: `[ start ..< end ]` – start inclusive, end exclusive
 *
 * ─── Omitted Bounds (Future Enhancement) ──────────────────────────────────
 *   Currently both start and end expressions are required. Future versions
 *   may support `[..end]`, `[start..]`, or `[..]` for full slice.
 *
 * ─── Array Types ──────────────────────────────────────────────────────────
 *   - Fixed arrays (`[N, T]`): slice creates a view
 *   - Dynamic arrays (`[*, T]`): slice creates a view
 *   - Slices (`[_, T]`): slice creates a sub-slice
 *
 * ─── Result Type ──────────────────────────────────────────────────────────
 *   The result of a slice is always a slice type `[_, T]`, regardless of
 *   whether the source was a fixed, dynamic, or slice array.
 *
 * ─── Error Recovery ───────────────────────────────────────────────────────
 *   - Missing '[': handled by caller (consume() reports error before entering)
 *   - Missing start expression: reports error, returns UnknownExprAST
 *   - Missing '..' or '..<' operator: reports error, returns UnknownExprAST
 *   - Missing end expression: reports error, returns UnknownExprAST
 *   - Missing closing ']': consume() reports error
 *
 * ─── Semantic Notes (Not Parser Responsibility) ───────────────────────────
 *   - Target must be an array type (fixed, dynamic, or slice)
 *   - Start and end expressions must be integer types
 *   - Bounds are checked at runtime
 *   - End must be >= start for exclusive slices
 *   - End must be >= start for inclusive slices
 */
ExprPtr Parser::parseSliceExpr(ExprPtr target) {
    LUC_LOG_EXPR_VERBOSE("parseSliceExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LBRACKET, "expected '['");

    // Parse start expression
    ExprPtr start = parseExpr();
    if (!start) {
        LUC_LOG_EXPR("parseSliceExpr: ERROR - expected start expression");
        errorAt(DiagCode::E1008, "expected start expression for slice");
        return arena_.make<UnknownExprAST>();
    }

    // Check for range operator
    if (!ts_.check(TokenType::RANGE)) {
        LUC_LOG_EXPR("parseSliceExpr: ERROR - expected '..' or '..<' after start expression");
        errorAt(DiagCode::E1001, "expected '..' or '..<' after start expression");
        return arena_.make<UnknownExprAST>();
    }
    ts_.advance(); // consume '..'
    
    // Check for exclusive syntax (..<)
    bool isExclusive = ts_.match(TokenType::LESS);
    LUC_LOG_EXPR_EXTREME("parseSliceExpr: slice is " << (isExclusive ? "exclusive" : "inclusive"));

    // Parse end expression
    ExprPtr end = parseExpr();
    if (!end) {
        LUC_LOG_EXPR("parseSliceExpr: ERROR - expected end expression");
        errorAt(DiagCode::E1008, "expected end expression for slice");
        return arena_.make<UnknownExprAST>();
    }

    ts_.consume(TokenType::RBRACKET, "expected ']' to close slice expression");
    
    auto node = arena_.make<SliceExprAST>();
    node->loc = loc;
    node->target = target;
    node->start = start;
    node->end = end;
    node->isExclusive = isExclusive;
    
    LUC_LOG_EXPR_VERBOSE("parseSliceExpr: success");
    return node;
}