#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

ExprPtr Parser::parsePipelineExpr(ExprPtr seed) {
    if (!seed) {
        errorAt(DiagCode::E2008, "expected pipeline seed before '|>'");
        return arena_.make<UnknownExprAST>();
    }

    std::vector<PipelineStepPtr> steps;  // temporary

    while (ts_.check(TokenType::PIPELINE)) {
        ts_.advance();
        PipelineStepPtr step = parsePipelineStep();
        if (step) {
            steps.push_back(std::move(step));
        } else {
            break;
        }
    }

    if (steps.empty()) {
        errorAt(DiagCode::E2006, "pipeline '|>' requires at least one step");
        return seed;
    }

    auto node = arena_.make<PipelineExprAST>();
    node->loc = seed->loc;
    node->seed = std::move(seed);
    
    auto builder = arena_.makeBuilder<PipelineStepPtr>();
    for (auto& s : steps) builder.push_back(std::move(s));
    node->steps = builder.build();

    return node;
}

PipelineStepPtr Parser::parsePipelineStep() {
    if (looksLikeAnonFunc()) {
        return parseAnonFuncPipelineStep();
    }

    bool isPrimitiveType = isPrimitiveTypeToken(ts_.peekType());
    if (!ts_.check(TokenType::IDENTIFIER) && !isPrimitiveType) {
        errorAt(DiagCode::E2002, "expected function name, method reference, or anonymous function");
        auto step = arena_.make<PipelineStepAST>();
        step->kind = PipelineStepKind::Ident;
        step->ident = pool_.intern("<error>");
        ts_.advance();
        return step;
    }

    std::string name = ts_.advance().value;
    ArenaSpan<TypePtr> genericArgs;
    if (ts_.check(TokenType::LESS)) {
        genericArgs = parseGenericArgs();
    }

    if (ts_.check(TokenType::COLON)) {
        return parseBehaviorPipelineStep(name, genericArgs);
    }
    
    if (ts_.check(TokenType::DOT)) {
        return parseFieldPipelineStep(name, genericArgs);
    }

    if (ts_.check(TokenType::LBRACKET)) {
        return parseIndexPipelineStep(name, genericArgs);
    }

    if (ts_.check(TokenType::LPAREN)) {
        return parseArgPackPipelineStep(name, genericArgs);
    }

    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::Ident;
    step->ident = pool_.intern(name);
    step->genericArgs = genericArgs;
    return step;
}

PipelineStepPtr Parser::parseAnonFuncPipelineStep() {
    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::AnonFunc;
    step->anonFunc = parseAnonFuncExpr();
    return step;
}

PipelineStepPtr Parser::parseBehaviorPipelineStep(const std::string& typeName, ArenaSpan<TypePtr> genericArgs) {
    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::BehaviorRef;
    step->typeName = pool_.intern(typeName);
    step->genericArgs = genericArgs;

    ts_.consume(TokenType::COLON, "expected ':'");
    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected method name after ':'");
        step->method = pool_.intern("<error>");
        return step;
    }
    step->method = pool_.intern(ts_.advance().value);

    if (ts_.check(TokenType::LPAREN)) {
        ts_.advance();
        std::vector<ExprPtr> packArgs;
        if (!ts_.check(TokenType::RPAREN)) packArgs = parseExprList(TokenType::RPAREN);
        ts_.consume(TokenType::RPAREN, "expected ')'");
        if (!ts_.match(TokenType::BANG)) {
            errorAt(DiagCode::E2001, "expected '!' after arguments for method argument pack");
            return step;
        }
        step->kind = PipelineStepKind::BehaviorArgPack;
        auto builder = arena_.makeBuilder<ExprPtr>();
        for (auto& a : packArgs) builder.push_back(std::move(a));
        step->packArgs = builder.build();
    }

    return step;
}

PipelineStepPtr Parser::parseFieldPipelineStep(const std::string& ident, ArenaSpan<TypePtr> genericArgs) {
    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::FieldRef;
    step->ident = pool_.intern(ident);
    step->genericArgs = genericArgs;

    ts_.consume(TokenType::DOT, "expected '.'");
    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected field name after '.'");
        step->field = pool_.intern("<error>");
        return step;
    }
    step->field = pool_.intern(ts_.advance().value);

    if (ts_.check(TokenType::LPAREN)) {
        ts_.advance();
        std::vector<ExprPtr> packArgs;
        if (!ts_.check(TokenType::RPAREN)) packArgs = parseExprList(TokenType::RPAREN);
        ts_.consume(TokenType::RPAREN, "expected ')'");
        if (!ts_.match(TokenType::BANG)) {
            errorAt(DiagCode::E2001, "expected '!' after arguments for field argument pack");
            return step;
        }
        step->kind = PipelineStepKind::FieldArgPack;
        auto builder = arena_.makeBuilder<ExprPtr>();
        for (auto& a : packArgs) builder.push_back(std::move(a));
        step->packArgs = builder.build();
    }

    return step;
}

PipelineStepPtr Parser::parseIndexPipelineStep(const std::string& ident, ArenaSpan<TypePtr> genericArgs) {
    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::IndexRef;
    step->ident = pool_.intern(ident);
    step->genericArgs = genericArgs;

    auto addIndex = [&](ExprPtr target, ExprPtr idx) -> ExprPtr {
        auto node = arena_.make<IndexExprAST>();
        node->target = std::move(target);
        node->index = std::move(idx);
        node->kind = IndexKind::Element;
        return node;
    };

    ts_.consume(TokenType::LBRACKET, "expected '['");
    ExprPtr idx = parseExpr();
    if (!idx) {
        errorAt(DiagCode::E2008, "expected index expression");
        int bracketDepth = 1;
        while (!ts_.isAtEnd() && bracketDepth > 0) {
            if (ts_.check(TokenType::LBRACKET)) ++bracketDepth;
            else if (ts_.check(TokenType::RBRACKET)) --bracketDepth;
            ts_.advance();
        }
        step->index = arena_.make<UnknownExprAST>();
        return step;
    }
    ts_.consume(TokenType::RBRACKET, "expected ']' after index");
    
    auto baseIdent = arena_.make<IdentifierExprAST>(pool_.intern(ident));
    baseIdent->loc = ts_.currentLoc();
    ExprPtr indexChain = addIndex(std::move(baseIdent), std::move(idx));

    while (ts_.check(TokenType::LBRACKET)) {
        ts_.advance();
        ExprPtr nextIdx = parseExpr();
        if (!nextIdx) {
            errorAt(DiagCode::E2008, "expected index expression");
            int bracketDepth = 1;
            while (!ts_.isAtEnd() && bracketDepth > 0) {
                if (ts_.check(TokenType::LBRACKET)) ++bracketDepth;
                else if (ts_.check(TokenType::RBRACKET)) --bracketDepth;
                ts_.advance();
            }
            break;
        }
        ts_.consume(TokenType::RBRACKET, "expected ']' after index");
        indexChain = addIndex(std::move(indexChain), std::move(nextIdx));
    }

    step->index = std::move(indexChain);

    if (ts_.check(TokenType::LPAREN)) {
        ts_.advance();
        std::vector<ExprPtr> packArgs;
        if (!ts_.check(TokenType::RPAREN)) packArgs = parseExprList(TokenType::RPAREN);
        ts_.consume(TokenType::RPAREN, "expected ')'");
        if (!ts_.match(TokenType::BANG)) {
            errorAt(DiagCode::E2001, "expected '!' after arguments for array index argument pack");
            return step;
        }
        step->kind = PipelineStepKind::IndexArgPack;
        auto builder = arena_.makeBuilder<ExprPtr>();
        for (auto& a : packArgs) builder.push_back(std::move(a));
        step->packArgs = builder.build();
    }

    return step;
}

PipelineStepPtr Parser::parseArgPackPipelineStep(const std::string& ident, ArenaSpan<TypePtr> genericArgs) {
    auto step = arena_.make<PipelineStepAST>();
    step->kind = PipelineStepKind::ArgPack;
    step->ident = pool_.intern(ident);
    step->genericArgs = genericArgs;

    ts_.consume(TokenType::LPAREN, "expected '('");
    std::vector<ExprPtr> packArgs;
    if (!ts_.check(TokenType::RPAREN)) packArgs = parseExprList(TokenType::RPAREN);
    ts_.consume(TokenType::RPAREN, "expected ')'");
    
    if (!ts_.match(TokenType::BANG)) {
        errorAt(DiagCode::E2001, "expected '!' after arguments for function argument pack");
        step->kind = PipelineStepKind::Ident;
        step->ident = pool_.intern("<error>");
        return step;
    }
    
    auto builder = arena_.makeBuilder<ExprPtr>();
    for (auto& a : packArgs) builder.push_back(std::move(a));
    step->packArgs = builder.build();
    return step;
}

ComposeOperandPtr Parser::parseComposeOperand() {
    auto op = arena_.make<ComposeOperandAST>();

    bool isPrimitiveType = isPrimitiveTypeToken(ts_.peekType());
    if (!ts_.check(TokenType::IDENTIFIER) && !isPrimitiveType) {
        errorAt(DiagCode::E2002, "expected function name or method reference");
        return nullptr;
    }

    std::string name = ts_.advance().value;

    if (ts_.check(TokenType::COLON) && ts_.peekNextType() == TokenType::IDENTIFIER) {
        ts_.advance();
        std::string method = ts_.advance().value;
        op->kind = ComposeOperandKind::BehaviorRef;
        op->typeName = pool_.intern(name);
        op->method = pool_.intern(method);
    } else if (ts_.check(TokenType::DOT) && ts_.peekNextType() == TokenType::IDENTIFIER) {
        ts_.advance();
        std::string field = ts_.advance().value;
        op->kind = ComposeOperandKind::FieldRef;
        op->ident = pool_.intern(name);
        op->field = pool_.intern(field);
    } else {
        op->kind = ComposeOperandKind::Ident;
        op->ident = pool_.intern(name);
    }

    return op;
}