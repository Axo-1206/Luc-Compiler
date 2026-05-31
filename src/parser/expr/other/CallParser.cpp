#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

ExprPtr Parser::parseCallExpr(ExprPtr callee, ArenaSpan<TypePtr> genericArgs) {
    SourceLocation loc = callee->loc;
    ts_.consume(TokenType::LPAREN, "expected '('");

    auto node = arena_.make<CallExprAST>();
    node->loc = loc;
    node->callee = std::move(callee);
    node->genericArgs = genericArgs;

    if (!ts_.check(TokenType::RPAREN)) {
        node->args = parseArgList();
    }

    ts_.consume(TokenType::RPAREN, "expected ')' to close argument list");
    node->isArgPack = ts_.match(TokenType::BANG);

    return node;
}