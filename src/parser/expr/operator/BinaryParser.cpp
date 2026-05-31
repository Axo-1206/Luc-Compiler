#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Infix Operator Handlers
// ============================================================================
// 
// These functions are called from parsePrattExpr when an infix operator is
// encountered. Each consumes the operator token, parses the right operand,
// and builds the corresponding AST node.
// 
//   parseInfixAssign()      : =, +=, -=, *=, /=, ^=, %=, &&=, ||=, ~^=, <<=, >>=
//   parseInfixIs()          : is (type check)
//   parseInfixNullCoalesce(): ?? (null coalescing)
//   parseInfixBinary()      : all other binary operators
// 
// Chained comparison detection: parseInfixBinary reports an error when it
// sees patterns like `a < b < c` (not allowed in Luc).
// ============================================================================

ExprPtr Parser::parseInfixAssign(ExprPtr lhs, bool allowStructLiteral) {
    TokenType opTok = ts_.advance().type;
    AssignOp op = tokenToAssignOp(opTok);
    
    ExprPtr rhs = parsePrattExpr(PREC_ASSIGN - 1, allowStructLiteral);
    if (!rhs) {
        errorAt(DiagCode::E2008, "expected expression after assignment operator");
        return lhs;
    }

    auto node = arena_.make<AssignExprAST>();
    node->loc = lhs->loc;
    node->op = op;
    node->lhs = std::move(lhs);
    node->rhs = std::move(rhs);
    return node;
}

ExprPtr Parser::parseInfixIs(ExprPtr lhs) {
    ts_.advance();
    TypePtr checkType = parseType();
    if (!checkType) {
        errorAt(DiagCode::E2005, "expected type after 'is'");
        return lhs;
    }

    auto node = arena_.make<IsExprAST>();
    node->loc = lhs->loc;
    node->expr = std::move(lhs);
    node->checkType = std::move(checkType);
    return node;
}

ExprPtr Parser::parseInfixNullCoalesce(ExprPtr lhs, bool allowStructLiteral) {
    ts_.advance();
    
    ExprPtr fallback = parsePrattExpr(PREC_NULLCOAL - 1, allowStructLiteral);
    if (!fallback) {
        errorAt(DiagCode::E2008, "expected expression after '\?\?'");
        return lhs;
    }

    auto node = arena_.make<NullCoalesceExprAST>();
    node->loc = lhs->loc;
    node->value = std::move(lhs);
    node->fallback = std::move(fallback);
    return node;
}

ExprPtr Parser::parseInfixBinary(ExprPtr lhs, TokenType opTok, int prec, bool allowStructLiteral) {
    ts_.advance();
    
    int nextPrec = (opTok == TokenType::POW) ? prec - 1 : prec;
    ExprPtr rhs = parsePrattExpr(nextPrec, allowStructLiteral);
    if (!rhs) {
        errorAt(DiagCode::E2008, "expected right-hand side of binary expression");
        return lhs;
    }

    // Chained comparison detection
    auto isComparisonOp = [](TokenType tt) {
        switch (tt) {
            case TokenType::EQUAL_EQUAL:
            case TokenType::EQUAL_EQUAL_EQUAL:
            case TokenType::NOT_EQUAL:
            case TokenType::LESS:
            case TokenType::GREATER:
            case TokenType::LESS_EQUAL:
            case TokenType::GREATER_EQUAL:
            case TokenType::IS:
                return true;
            default:
                return false;
        }
    };

    if (isComparisonOp(opTok) && isComparisonOp(ts_.peekType())) {
        errorAt(DiagCode::E2002, "chained comparisons not allowed; use 'and' explicitly");
    }

    auto node = arena_.make<BinaryExprAST>();
    node->loc = lhs->loc;
    node->op = tokenToBinaryOp(opTok);
    node->left = std::move(lhs);
    node->right = std::move(rhs);
    return node;
}