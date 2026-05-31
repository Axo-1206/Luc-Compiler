#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

ExprPtr Parser::parseComposeExpr(ExprPtr lhs) {
    auto node = arena_.make<ComposeExprAST>();
    node->loc = lhs->loc;
    node->left = std::move(lhs);

    std::vector<ComposeOperandPtr> operands;

    while (ts_.check(TokenType::COMPOSE)) {
        ts_.advance();
        ComposeOperandPtr op = parseComposeOperand();
        if (!op) {
            errorAt(DiagCode::E2002, "expected function name after '+>'");
            break;
        }
        operands.push_back(std::move(op));
    }

    if (operands.empty()) {
        return std::move(node->left);
    }

    // Build ArenaSpan
    auto builder = arena_.makeBuilder<ComposeOperandPtr>();
    for (auto& op : operands) builder.push_back(std::move(op));
    node->operands = builder.build();

    return node;
}