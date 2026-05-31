#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Intrinsic Call
// ============================================================================
// 
// Intrinsic calls use the '#' prefix: #sizeof(T), #sqrt(x), #memcpy(dst, src, n)
// 
// Two categories:
//   - Type intrinsics : #sizeof, #alignof – take a type argument
//   - Value intrinsics: all others – take expression arguments
// 
// Detection is based on the intrinsic name after '#'.
// Arguments are parsed as expressions (or a single type for type intrinsics).
// ============================================================================

ExprPtr Parser::parseIntrinsicCallExpr() {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::HASH, "expected '#'");

    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected intrinsic name after '#'");
        return arena_.make<UnknownExprAST>();
    }

    auto node = arena_.make<IntrinsicCallExprAST>();
    node->loc = loc;
    node->intrinsicName = pool_.intern(ts_.advance().value);

    if (!ts_.check(TokenType::LPAREN)) {
        errorAt(DiagCode::E2001, "expected '(' after intrinsic name");
        return arena_.make<UnknownExprAST>();
    }
    ts_.advance();

    std::string intrinsicStr = std::string(pool_.lookup(node->intrinsicName));
    bool isTypeIntrinsic = (intrinsicStr == "sizeof" || intrinsicStr == "alignof");

    if (isTypeIntrinsic) {
        if (ts_.check(TokenType::RPAREN)) {
            errorAt(DiagCode::E2005, "expected type argument");
        } else {
            TypePtr typeArg = parseType();
            if (!typeArg) errorAt(DiagCode::E2005, "invalid type argument");
            else node->typeArg = std::move(typeArg);
        }
        ts_.consume(TokenType::RPAREN, "expected ')' after type argument");
    } else {
        std::vector<ExprPtr> args;
        while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
            size_t savedPos = ts_.getPos();
            ExprPtr arg = parseExpr();
            if (ts_.getPos() == savedPos) {
                errorAt(DiagCode::E2008, "expected argument expression");
                if (!ts_.isAtEnd()) ts_.advance();
                break;
            }
            args.push_back(std::move(arg));
            if (ts_.check(TokenType::RPAREN)) break;
            if (!ts_.match(TokenType::COMMA)) {
                errorAt(DiagCode::E2001, "expected ',' or ')' in intrinsic argument list");
                break;
            }
        }
        auto builder = arena_.makeBuilder<ExprPtr>();
        for (auto& a : args) builder.push_back(std::move(a));
        node->args = builder.build();
        ts_.consume(TokenType::RPAREN, "expected ')' to close intrinsic call");
    }

    return node;
}