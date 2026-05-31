#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Range Expression
// ============================================================================
// 
// parseRangeExpr() parses an inclusive or exclusive range.
// 
// Grammar: expr ( '..' | '..<' ) expr
// 
// Examples:
//   0..10    – inclusive range (0 through 10)
//   1..<5    – exclusive range (1 through 4)
// 
// ─── Usage Contexts ────────────────────────────────────────────────────────
// Ranges appear in:
//   - For loops          : for i in 0..10 { ... }
//   - Match patterns     : case 1..10 => "light"
//   - Slice indices      : nums[1..3]
// 
// ─── Preconditions ─────────────────────────────────────────────────────────
// - Called when '..' or '..<' is found after the lo expression
// - The lo expression is already parsed and passed as a parameter
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at '..' or '..<' (lo already consumed)
// On exit:  positioned after the hi expression
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
// - Missing hi expression after '..' → returns UnknownExprAST
// ============================================================================

ExprPtr Parser::parseRangeExpr(ExprPtr lo, bool allowStructLiteral) {
    SourceLocation loc = lo->loc;
    ts_.consume(TokenType::RANGE, "expected '..'");

    bool isExclusive = ts_.match(TokenType::LESS);
    ExprPtr hi = parsePrattExpr(PREC_ADD, allowStructLiteral);
    if (!hi) {
        errorAt(DiagCode::E2008, "expected upper bound after '..'");
        return arena_.make<UnknownExprAST>();
    }

    auto node = arena_.make<RangeExprAST>();
    node->loc = loc;
    node->lo = std::move(lo);
    node->hi = std::move(hi);
    node->isExclusive = isExclusive;
    return node;
}