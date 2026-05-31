#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Type Conversion Expression (Cast)
// ============================================================================
// 
// parseTypeConvExpr() parses explicit type casts.
// 
// Grammar:
//   Safe cast:   type_name '(' expr ')'
//   Unsafe cast: '*' type_name '(' expr ')'
// 
// Examples:
//   float(x)      – safe cast (widening, enum→int, int→string)
//   *uint32(bits) – unsafe bit reinterpret (only in @extern)
// 
// ─── Safe vs Unsafe ────────────────────────────────────────────────────────
//   Safe (isUnsafe = false): type_name '(' expr ')'
//     - Supported casts: primitive widening, enum→int, int→string
//     - Validated by semantic pass
// 
//   Unsafe (isUnsafe = true): '*' type_name '(' expr ')'
//     - Bit reinterpretation
//     - Only allowed inside @extern functions or with --unsafe flag
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: 
//   - For unsafe cast: positioned after '*' and type name (at '(')
//   - For safe cast: positioned after type name (at '(')
// On exit: positioned after the closing ')'
// 
// ─── Preconditions ─────────────────────────────────────────────────────────
// - The caller (parsePrimaryExpr) has already consumed the type name
// - The current token is '('
// ============================================================================

ExprPtr Parser::parseTypeConvExpr(bool isUnsafe, TypePtr targetType) {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LPAREN, "expected '(' for explicit type cast");

    ExprPtr expr = parseExpr();
    if (!expr) {
        errorAt(DiagCode::E2008, "expected expression inside explicit type cast");
        return arena_.make<UnknownExprAST>();
    }

    ts_.consume(TokenType::RPAREN, "expected ')' to close explicit type cast");

    auto node = arena_.make<TypeConvExprAST>(std::move(targetType), std::move(expr), isUnsafe);
    node->loc = loc;
    return node;
}