#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// If Expression (Expression Form)
// ============================================================================
// 
// parseIfExpr() parses the expression form of 'if', which produces a value.
// 
// Grammar: 'if' expr '??' expr 'else' expr
// 
// Example: let grade = if score >= 60 ?? "pass" else "fail"
// 
// ─── Comparison with IfStmtAST ────────────────────────────────────────────
//   IfExprAST (this)              | IfStmtAST (in ParserStmt.cpp)
//   ------------------------------|------------------------------------------
//   Expression context            | Statement context
//   'else' required               | 'else' optional
//   Produces a value              | No value produced
//   Uses '??' separator           | No '??' separator
// 
// ─── Operator Precedence ──────────────────────────────────────────────────
// The '??' here is a syntactic separator (not the null‑coalescing operator).
// The condition is parsed with PREC_NULLCOAL to stop at the first '??'.
// The expression is right‑associative: a ?? b else c ?? d else e.
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at 'if' keyword
// On exit:  positioned after the else branch expression
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
// - Missing condition after 'if' → returns UnknownExprAST
// - Missing '??' after condition → reports error, returns UnknownExprAST
// - Missing 'else' keyword → reports error, returns UnknownExprAST
// ============================================================================

ExprPtr Parser::parseIfExpr(bool allowStructLiteral) {
    LUC_LOG_EXPR_VERBOSE("parseIfExpr: entering (expression form)");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::IF, "expected 'if'");

    ExprPtr condition = parsePrattExpr(PREC_NULLCOAL, allowStructLiteral);
    if (!condition) {
        LUC_LOG_EXPR("parseIfExpr: ERROR - expected condition after 'if'");
        errorAt(DiagCode::E1008, "expected condition after 'if'");
        return arena_.make<UnknownExprAST>();
    }
    LUC_LOG_EXPR_EXTREME("parseIfExpr: condition parsed");

    if (!ts_.match(TokenType::QUESTION_QUESTION)) {
        LUC_LOG_EXPR("parseIfExpr: ERROR - expected '\?\?' after condition");
        errorAt(DiagCode::E1001, "expected '\?\?' after if condition in expression form");
        return arena_.make<UnknownExprAST>();
    }
    LUC_LOG_EXPR_EXTREME("parseIfExpr: '\?\?' separator found");

    ExprPtr thenBranch = parseExpr();
    if (!thenBranch) {
        LUC_LOG_EXPR("parseIfExpr: ERROR - expected expression after '\?\?'");
        errorAt(DiagCode::E1008, "expected expression after '\?\?'");
    } else {
        LUC_LOG_EXPR_EXTREME("parseIfExpr: then branch parsed");
    }

    if (!ts_.match(TokenType::ELSE)) {
        LUC_LOG_EXPR("parseIfExpr: ERROR - expression-form 'if' requires 'else' branch");
        errorAt(DiagCode::E1006, "expression-form 'if' requires an 'else' branch");
        return arena_.make<UnknownExprAST>();
    }
    LUC_LOG_EXPR_EXTREME("parseIfExpr: 'else' keyword found");

    ExprPtr elseBranch = parseExpr();
    if (!elseBranch) {
        LUC_LOG_EXPR("parseIfExpr: ERROR - expected expression after 'else'");
        errorAt(DiagCode::E1008, "expected expression after 'else'");
    } else {
        LUC_LOG_EXPR_EXTREME("parseIfExpr: else branch parsed");
    }

    auto node = arena_.make<IfExprAST>();
    node->loc = loc;
    node->condition = std::move(condition);
    node->thenBranch = std::move(thenBranch);
    node->elseBranch = std::move(elseBranch);
    
    LUC_LOG_EXPR_VERBOSE("parseIfExpr: success");
    return node;
}