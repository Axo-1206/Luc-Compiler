/**
 * @file LiteralParser.cpp
 * @brief Parses literal expressions, array/struct literals, and anonymous functions.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements parsing of atomic expression values that appear in
 * primary expression position. These include scalar literals, array literals,
 * struct literals, and anonymous functions.
 * 
 * Grammar (from LUC_GRAMMAR.md):
 *   literal         := INT_LITERAL | FLOAT_LITERAL | STRING_LITERAL
 *                    | RAW_STRING_LITERAL | CHAR_LITERAL | HEX_LITERAL
 *                    | BINARY_LITERAL | 'true' | 'false' | 'nil'
 * 
 *   array_literal   := '[' [ expr { ',' expr } ] ']'
 * 
 *   struct_literal  := IDENTIFIER [ '<' type { ',' type } '>' ] '{' { field_init } '}'
 *   field_init      := IDENTIFIER '=' expr
 * 
 *   anon_func       := param_group { param_group } [ '->' return_list ] block
 * 
 * @see ParserExpr.cpp for expression dispatch
 * @see ParserType.cpp for type parsing
 * @see ParserStmt.cpp for block parsing
 */

#include "ast/BaseAST.hpp"
#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Literal Expression
// ============================================================================

ExprPtr Parser::parseLiteralExpr() {
    LUC_LOG_EXPR_EXTREME("parseLiteralExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    Token tok = ts_.advance();

    LiteralKind kind;
    switch (tok.type) {
        case TokenType::INT_LITERAL:        kind = LiteralKind::Int; break;
        case TokenType::FLOAT_LITERAL:      kind = LiteralKind::Float; break;
        case TokenType::STRING_LITERAL:     kind = LiteralKind::String; break;
        case TokenType::RAW_STRING_LITERAL: kind = LiteralKind::RawString; break;
        case TokenType::CHAR_LITERAL:       kind = LiteralKind::Char; break;
        case TokenType::HEX_LITERAL:        kind = LiteralKind::Hex; break;
        case TokenType::BINARY_LITERAL:     kind = LiteralKind::Binary; break;
        case TokenType::TRUE:               kind = LiteralKind::True; break;
        case TokenType::FALSE:              kind = LiteralKind::False; break;
        case TokenType::NIL:                kind = LiteralKind::Nil; break;
        default:
            LUC_LOG_EXPR("parseLiteralExpr: ERROR - non-literal token");
            errorAt(DiagCode::E1002, "internal error: parseLiteralExpr on non-literal token");
            return arena_.make<UnknownExprAST>();
    }

    LUC_LOG_EXPR_EXTREME("parseLiteralExpr: value = '" << tok.value << "', kind = " << (int)kind);
    auto node = arena_.make<LiteralExprAST>(kind, pool_.intern(tok.value));
    node->loc = loc;
    return node;
}

// ============================================================================
// Array Literal
// ============================================================================

ExprPtr Parser::parseArrayLiteralExpr() {
    LUC_LOG_EXPR_VERBOSE("parseArrayLiteralExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LBRACKET, "expected '['");

    std::vector<ExprPtr> elements;
    int elemCount = 0;

    while (!ts_.check(TokenType::RBRACKET) && !ts_.isAtEnd()) {
        ts_.match(TokenType::COMMA);
        if (ts_.check(TokenType::RBRACKET)) break;

        size_t beforePos = ts_.getPos();
        ExprPtr elem = parseExpr();
        if (ts_.getPos() == beforePos) {
            LUC_LOG_EXPR("parseArrayLiteralExpr: ERROR - expected expression inside array literal");
            errorAt(DiagCode::E1008, "expected expression inside array literal");
            if (!ts_.isAtEnd()) ts_.advance();
            break;
        }
        elemCount++;
        LUC_LOG_EXPR_EXTREME("parseArrayLiteralExpr: element #" << elemCount);
        elements.push_back(std::move(elem));
    }

    ts_.consume(TokenType::RBRACKET, "expected ']' to close array literal");

    auto node = arena_.make<ArrayLiteralExprAST>();
    node->loc = loc;

    auto builder = arena_.makeBuilder<ExprPtr>();
    for (auto& e : elements) builder.push_back(std::move(e));
    node->elements = builder.build();

    LUC_LOG_EXPR_VERBOSE("parseArrayLiteralExpr: " << elemCount << " element(s)");
    return node;
}

// ============================================================================
// Struct Literal
// ============================================================================

ExprPtr Parser::parseStructLiteralExpr(InternedString typeName, ArenaSpan<TypePtr> genericArgs) {
    LUC_LOG_EXPR_VERBOSE("parseStructLiteralExpr: type = " << pool_.lookup(typeName) 
                         << ", generic args = " << genericArgs.size()
                         << " at line " << ts_.currentLoc().line()
                         << ", col " << ts_.currentLoc().column());
    
    auto node = arena_.make<StructLiteralExprAST>();
    node->loc = ts_.currentLoc();
    node->typeName = typeName;
    node->genericArgs = genericArgs;
    
    ts_.consume(TokenType::LBRACE, "expected '{' to start struct literal");
    
    // Parse field initializers
    std::vector<FieldInitPtr> inits;
    while (!ts_.check(TokenType::RBRACE) && !ts_.isAtEnd()) {
        if (ts_.check(TokenType::IDENTIFIER)) {
            SourceLocation fieldLoc = ts_.currentLoc();
            Token fieldTok = ts_.advance();
            ts_.consume(TokenType::ASSIGN, "expected '=' in field initializer");
            ExprPtr value = parseExpr();
            
            auto init = arena_.make<FieldInitAST>();
            init->loc = fieldLoc;
            init->name = pool_.intern(fieldTok.value);
            init->value = std::move(value);
            inits.push_back(std::move(init));
            
            ts_.match(TokenType::COMMA);
        } else {
            errorAt(DiagCode::E1003, "expected field name in struct literal");
            break;
        }
    }
    
    auto builder = arena_.makeBuilder<FieldInitPtr>();
    for (auto& init : inits) builder.push_back(std::move(init));
    node->inits = builder.build();
    
    ts_.consume(TokenType::RBRACE, "expected '}' to close struct literal");
    
    return node;
}

// ============================================================================
// Anonymous Function Expression
// ============================================================================

ExprPtr Parser::parseAnonFuncExpr() {
    LUC_LOG_EXPR_VERBOSE("parseAnonFuncExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    
    auto node = arena_.make<AnonFuncExprAST>();
    node->loc = loc;
    
    // Anonymous functions cannot have qualifiers – grammar rule.
    if (ts_.check(TokenType::TILDE)) {
        LUC_LOG_EXPR("parseAnonFuncExpr: ERROR - anonymous function cannot have qualifiers");
        errorAt(DiagCode::E1006, "anonymous function cannot have qualifiers");
        // Skip all qualifiers to recover
        while (ts_.check(TokenType::TILDE)) {
            ts_.advance();
            if (!ts_.check(TokenType::IDENTIFIER)) {
                errorAt(DiagCode::E1003, "expected qualifier name after '~'");
                break;
            }
            ts_.advance();
        }
    }
    
    if (!ts_.check(TokenType::LPAREN)) {
        LUC_LOG_EXPR("parseAnonFuncExpr: ERROR - expected '(' for parameters");
        errorAt(DiagCode::E1001, "expected '(' to start anonymous function parameters");
        return arena_.make<UnknownExprAST>();
    }
    
    // Parameter groups: flat accumulation
    std::vector<ParamPtr> allParams;
    std::vector<size_t> groupSizes;
    int groupCount = 0;
    
    while (ts_.check(TokenType::LPAREN)) {
        groupCount++;
        ParamGroup group = parseParamGroup();
        groupSizes.push_back(group.size());
        LUC_LOG_EXPR_EXTREME("parseAnonFuncExpr: group #" << groupCount 
                             << " has " << group.size() << " parameter(s)");
        for (auto& p : group) {
            allParams.push_back(std::move(p));
        }
    }
    
    auto paramsBuilder = arena_.makeBuilder<ParamPtr>();
    for (auto& p : allParams) paramsBuilder.push_back(std::move(p));
    node->sig.allParams = paramsBuilder.build();
    
    auto gsBuilder = arena_.makeBuilder<size_t>();
    for (auto& sz : groupSizes) gsBuilder.push_back(sz);
    node->sig.groupSizes = gsBuilder.build();
    
    LUC_LOG_EXPR_EXTREME("parseAnonFuncExpr: total " << allParams.size() << " parameters");
    
    // Optional return types
    if (ts_.check(TokenType::ARROW)) {
        ts_.advance();
        LUC_LOG_EXPR_EXTREME("parseAnonFuncExpr: parsing return types");
        node->sig.returnTypes = parseReturnList();
        LUC_LOG_EXPR_EXTREME("parseAnonFuncExpr: " << node->sig.returnTypes.size() << " return type(s)");
        if (node->sig.returnTypes.empty() && !ts_.check(TokenType::LBRACE)) {
            LUC_LOG_EXPR("parseAnonFuncExpr: ERROR - expected return type after '->'");
            errorAt(DiagCode::E1005, "expected return type after '->' in anonymous function");
        }
    }
    
    if (!ts_.check(TokenType::LBRACE)) {
        LUC_LOG_EXPR("parseAnonFuncExpr: ERROR - expected '{' for body");
        errorAt(DiagCode::E1001, "expected '{' to start anonymous function body");
        return arena_.make<UnknownExprAST>();
    }
    node->body = parseBlock();
    
    LUC_LOG_EXPR_VERBOSE("parseAnonFuncExpr: success");
    return node;
}