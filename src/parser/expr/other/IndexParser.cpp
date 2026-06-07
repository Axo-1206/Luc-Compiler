/**
 * @file IndexParser.cpp
 * @brief Parses array element access expressions.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements parsing of array element indexing operations.
 * The `parseIndexExpr()` function is called from `parsePostfixExpr()` when a
 * `[` token is encountered after an array‑typed expression, and the content
 * is a single expression (not a range).
 * 
 * Grammar (from LUC_GRAMMAR.md):
 *   postfix_op := '[' expr ']'                    -- element access
 * 
 * Examples:
 *   nums[2]                    → element access (index 2)
 *   matrix[row][col]           → nested index (handled by repeated calls)
 *   values[i] = 42             → indexed assignment (lvalue context)
 * 
 * @see SliceParser.cpp for slice access (range syntax)
 * @see ParserExpr.cpp for parsePostfixExpr integration
 * @see IndexExprAST in ExprAST.hpp for AST representation
 */

#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Index Expression Parser (Element Access)
// ============================================================================

/**
 * @brief Parses an array element access expression.
 *
 * Grammar:
 *   index_expr := '[' expr ']'
 *
 * This function consumes a single bracketed expression and produces an
 * IndexExprAST node.
 *
 * @param target The array‑typed expression being indexed (already parsed).
 * @return ExprPtr – IndexExprAST on success, or UnknownExprAST on error.
 *
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at '['
 * On exit:  positioned after the closing ']'
 *
 * ─── Element Access (Single Index) ────────────────────────────────────────
 *   Format: `[ index ]`
 *   - Parses a single expression as the index.
 *   - Returns IndexExprAST with the parsed index.
 *
 * ─── Array Types and Bounds ───────────────────────────────────────────────
 *   - Fixed arrays (`[N, T]`): bounds checked at compile time (if constant)
 *   - Dynamic arrays (`[*, T]`): bounds checked at runtime (returns nil on OOB)
 *   - Slices (`[_, T]`): bounds checked at runtime (panics on OOB)
 *
 * ─── Error Recovery ───────────────────────────────────────────────────────
 *   - Missing '[': handled by caller (consume() reports error before entering)
 *   - Missing index expression: reports error, returns UnknownExprAST
 *   - Missing closing ']': consume() reports error
 *
 * ─── Semantic Notes (Not Parser Responsibility) ───────────────────────────
 *   - Target must be an array type (fixed, dynamic, or slice)
 *   - Index expression must be integer type (int, uint, etc.)
 *   - Result type is the element type T
 */
ExprPtr Parser::parseIndexExpr(ExprPtr target) {
    LUC_LOG_EXPR_VERBOSE("parseIndexExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LBRACKET, "expected '['");

    ExprPtr index = parseExpr();
    if (!index) {
        LUC_LOG_EXPR("parseIndexExpr: ERROR - expected index expression");
        errorAt(DiagCode::E1008, "expected index expression");
        return arena_.make<UnknownExprAST>();
    }

    ts_.consume(TokenType::RBRACKET, "expected ']' to close index expression");
    
    auto node = arena_.make<IndexExprAST>();
    node->loc = loc;
    node->target = target;
    node->index = index;
    
    LUC_LOG_EXPR_VERBOSE("parseIndexExpr: success");
    return node;
}