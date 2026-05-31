#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// List helpers (temporary vector builders)
// ============================================================================

std::vector<ExprPtr> Parser::parseExprList(TokenType endType) {
    std::vector<ExprPtr> list;
    while (!ts_.check(endType) && !ts_.isAtEnd()) {
        if (!list.empty() && !ts_.match(TokenType::COMMA))
            break;
        ExprPtr expr = parseExpr();
        if (expr) list.push_back(std::move(expr));
    }
    ts_.consume(endType, "expected '" + LucDebug::tokenTypeToString(endType) + "'");
    return list;
}

std::vector<TypePtr> Parser::parseTypeList(TokenType endType) {
    std::vector<TypePtr> list;
    while (!ts_.check(endType) && !ts_.isAtEnd()) {
        if (!list.empty() && !ts_.match(TokenType::COMMA))
            break;
        TypePtr ty = parseType();
        if (ty) list.push_back(std::move(ty));
    }
    ts_.consume(endType, "expected '" + LucDebug::tokenTypeToString(endType) + "'");
    return list;
}

std::vector<StmtPtr> Parser::parseStmtList(TokenType endType) {
    std::vector<StmtPtr> list;
    while (!ts_.check(endType) && !ts_.isAtEnd()) {
        StmtPtr stmt = parseStmt();
        if (stmt) list.push_back(std::move(stmt));
    }
    ts_.consume(endType, "expected '" + LucDebug::tokenTypeToString(endType) + "'");
    return list;
}

std::vector<ParamPtr> Parser::parseParamList() {
    std::vector<ParamPtr> list;
    while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
        if (!list.empty() && !ts_.match(TokenType::COMMA))
            break;
        if (!ts_.check(TokenType::IDENTIFIER)) {
            errorAt(DiagCode::E2003, "expected parameter name");
            break;
        }
        auto param = arena_.make<ParamAST>();
        param->name = pool_.intern(ts_.advance().value);
        param->isVariadic = ts_.match(TokenType::VARIADIC);
        param->type = parseType();
        if (param->type) list.push_back(std::move(param));
    }
    return list;
}

ArenaSpan<ExprPtr> Parser::parseArgList() {
    std::vector<ExprPtr> args;
    int consecutiveErrors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;

    while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
        if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
            errorAt(DiagCode::E2002, "too many consecutive errors in argument list; skipping to ')'");
            while (!ts_.isAtEnd() && !ts_.check(TokenType::RPAREN)) ts_.advance();
            break;
        }

        size_t savedPos = ts_.getPos();
        ExprPtr arg = parseExpr();

        if (ts_.getPos() == savedPos) {
            errorAt(DiagCode::E2008, "expected argument expression");
            if (!ts_.isAtEnd()) ts_.advance();
            consecutiveErrors++;
            if (ts_.check(TokenType::COMMA)) ts_.advance();
            continue;
        }

        consecutiveErrors = 0;
        args.push_back(std::move(arg));

        if (ts_.check(TokenType::RPAREN)) break;
        if (!ts_.match(TokenType::COMMA)) {
            errorAt(DiagCode::E2001, "expected ',' after argument");
            while (!ts_.isAtEnd() && !ts_.check(TokenType::COMMA) && !ts_.check(TokenType::RPAREN)) {
                ts_.advance();
            }
            if (ts_.check(TokenType::COMMA)) ts_.advance();
            break;
        }
    }
    
    auto builder = arena_.makeBuilder<ExprPtr>();
    for (auto& a : args) builder.push_back(std::move(a));
    return builder.build();
}

// ============================================================================
// Parameter Group Parsing
// ============================================================================
// 
// parseParamGroup() is called for each `( param_list )` in function types and
// function declarations.
// 
// Grammar: '(' [ param_list ] ')'
// 
// The function is implemented in Parser.cpp but documented here for context.
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at '('
// On exit:  positioned after the closing ')'
//
// ─── Return Value ─────────────────────────────────────────────────────────
//   Returns ParamGroup (std::vector<ParamPtr>) – temporary collection
//   Caller is responsible for converting to ArenaSpan using SpanBuilder
//
// ─── Loop Safety ──────────────────────────────────────────────────────────
// Uses parseParamList() which has its own loop safety.
//
// ─── Error Recovery ───────────────────────────────────────────────────────
// - Missing '(': consume() reports error
// - Missing ')': consume() reports error
//
// ============================================================================

ParamGroup Parser::parseParamGroup() {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LPAREN, "expected '(' to start parameter group");
    
    std::vector<ParamPtr> params = parseParamList();
    
    ts_.consume(TokenType::RPAREN, "expected ')' to close parameter group");
    
    return params;
}

/**
 * @brief Parses the return list after '->' in function signatures.
 * 
 * Grammar:
 *   return_list := '(' [ return_type { ',' return_type } ] ')'   -- multiple
 *                | return_type                                    -- single
 * 
 * where `return_type` can itself be a function type with its own '->'.
 * 
 * Examples:
 *   -> int                                    -- single return
 *   -> (int, string)                          -- multi-return
 *   -> (x int) -> int                         -- function type
 *   -> (Result, int)                          -- multi-return with named type
 *   -> ((x int) -> int, string)               -- multi-return with function type
 *   -> () -> int                              -- zero-parameter function type
 * 
 * ─── Detection Strategy ─────────────────────────────────────────────────────
 *   To distinguish between multi-return `(Type, Type)` and function type
 *   `(param Type) -> Ret`, the parser:
 * 
 *   1. Looks inside the parentheses
 *   2. If it sees `IDENTIFIER` followed by a type start (parameter pattern)
 *      and later finds `->` after a complete parameter group, it parses as
 *      a function type
 *   3. Otherwise, it parses as a multi-return list
 * 
 *   This correctly handles ambiguous cases like `(Result, int)` where `Result`
 *   is a named type, not a parameter name.
 * 
 * ─── Function Type Detection Details ───────────────────────────────────────
 *   - Empty parentheses `()` are treated as a function type (zero parameters)
 *   - Single identifier without following type is NOT a function type
 *   - The presence of `->` after parameter group(s) confirms function type
 *   - Uses temporary parsing to test hypothesis without consuming tokens
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at the token after '->' (may be '(' or type start)
 * On exit:  positioned after the return list
 * 
 * ─── Return Value ─────────────────────────────────────────────────────────
 *   Returns ArenaSpan<TypePtr> – empty span indicates void function.
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing return type: reports error, returns empty span
 * - Missing ')' in multi-return: consume() reports error
 * - Invalid type in list: skips type, continues
 * 
 * @return ArenaSpan<TypePtr> – span of return types (empty = void)
 */
ArenaSpan<TypePtr> Parser::parseReturnList() {
    // Helper to check if a token type can start a type
    auto isTypeStart = [this](TokenType tt) -> bool {
        return isPrimitiveTypeToken(tt) ||
               tt == TokenType::IDENTIFIER ||
               tt == TokenType::LBRACKET ||
               tt == TokenType::AMPERSAND ||
               tt == TokenType::MUL ||
               tt == TokenType::LPAREN ||
               tt == TokenType::TILDE;
    };

    // Case 1: No parentheses → single return type
    if (!ts_.check(TokenType::LPAREN)) {
        TypePtr t = parseType();
        if (!t || t->isa<UnknownTypeAST>()) {
            errorAt(DiagCode::E2005, "expected return type after '->'");
            return ArenaSpan<TypePtr>();
        }
        auto builder = arena_.makeBuilder<TypePtr>();
        builder.push_back(std::move(t));
        return builder.build();
    }

    // We have '(' - need to determine if it's a function type or multi-return
    // Strategy: Peek inside and look for '->' after a complete parameter group
    
    size_t savedPos = ts_.getPos();
    const auto& tokens = ts_.getTokens();
    size_t tokenCount = ts_.getTokenCount();
    
    // Try to parse as a function type first
    // We'll attempt to parse a complete function type and see if it succeeds
    // without consuming tokens permanently
    
    // Create a temporary copy of the parser position
    size_t testPos = savedPos;
    
    // Skip comments at the start
    testPos = ts_.skipCommentsFrom(testPos);
    
    // Check if the '(' is followed by something that looks like a parameter group
    if (testPos < tokenCount && tokens[testPos].type == TokenType::LPAREN) {
        ++testPos; // consume '('
        testPos = ts_.skipCommentsFrom(testPos);
        
        // If we see a closing ')' immediately, this could be empty function type
        bool isEmptyParen = (testPos < tokenCount && tokens[testPos].type == TokenType::RPAREN);
        
        if (!isEmptyParen) {
            // Need to see if this looks like a parameter: IDENTIFIER followed by type start
            // vs just a type name (multi-return case)
            
            // Check if the next token is an identifier AND the token after that starts a type
            bool looksLikeParameter = false;
            if (testPos < tokenCount && tokens[testPos].type == TokenType::IDENTIFIER) {
                size_t afterIdent = testPos + 1;
                afterIdent = ts_.skipCommentsFrom(afterIdent);
                if (afterIdent < tokenCount && isTypeStart(tokens[afterIdent].type)) {
                    // This looks like a parameter: "name Type"
                    looksLikeParameter = true;
                }
            }
            
            if (looksLikeParameter) {
                // Try to parse a complete function type
                // We need to see if there's a '->' after the parameter group(s)
                
                // Simulate parsing up to the closing ')' of the first parameter group
                int parenDepth = 1;
                size_t paramEnd = testPos;
                while (paramEnd < tokenCount && parenDepth > 0) {
                    paramEnd = ts_.skipCommentsFrom(paramEnd);
                    if (paramEnd >= tokenCount) break;
                    TokenType tt = tokens[paramEnd].type;
                    if (tt == TokenType::LPAREN) ++parenDepth;
                    else if (tt == TokenType::RPAREN) --parenDepth;
                    ++paramEnd;
                }
                
                // Check after the closing ')' for '->' or more parameter groups
                size_t afterParams = paramEnd;
                afterParams = ts_.skipCommentsFrom(afterParams);
                
                // Look for additional parameter groups or '->'
                bool hasArrow = false;
                while (afterParams < tokenCount) {
                    afterParams = ts_.skipCommentsFrom(afterParams);
                    if (afterParams >= tokenCount) break;
                    TokenType tt = tokens[afterParams].type;
                    if (tt == TokenType::ARROW) {
                        hasArrow = true;
                        break;
                    }
                    if (tt == TokenType::LPAREN) {
                        // More parameter groups - continue scanning
                        ++afterParams;
                        continue;
                    }
                    break;
                }
                
                if (hasArrow) {
                    // This is definitely a function type
                    TypePtr funcType = parseFuncType();
                    if (!funcType || funcType->isa<UnknownTypeAST>()) {
                        errorAt(DiagCode::E2005, "expected function type");
                        return ArenaSpan<TypePtr>();
                    }
                    auto builder = arena_.makeBuilder<TypePtr>();
                    builder.push_back(std::move(funcType));
                    return builder.build();
                }
            }
        } else {
            // Empty parentheses - this is a function type with zero parameters
            // Check if there's a '->' after the closing ')'
            size_t afterParen = testPos + 1; // skip the ')'
            afterParen = ts_.skipCommentsFrom(afterParen);
            if (afterParen < tokenCount && tokens[afterParen].type == TokenType::ARROW) {
                TypePtr funcType = parseFuncType();
                if (!funcType || funcType->isa<UnknownTypeAST>()) {
                    errorAt(DiagCode::E2005, "expected function type");
                    return ArenaSpan<TypePtr>();
                }
                auto builder = arena_.makeBuilder<TypePtr>();
                builder.push_back(std::move(funcType));
                return builder.build();
            }
        }
    }
    
    // Not a function type (or detection inconclusive) → parse as multi-return list
    // Restore position and parse as multi-return
    ts_.setPos(savedPos);
    ts_.advance(); // consume '('
    
    std::vector<TypePtr> types;
    
    while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
        if (!types.empty() && !ts_.match(TokenType::COMMA)) {
            // Missing comma - try to continue
            if (ts_.check(TokenType::RPAREN)) break;
            errorAt(DiagCode::E2001, "expected ',' between return types");
            // Skip to next comma or closing parenthesis
            while (!ts_.isAtEnd() && !ts_.check(TokenType::COMMA) && !ts_.check(TokenType::RPAREN)) {
                ts_.advance();
            }
            continue;
        }
        
        if (ts_.check(TokenType::RPAREN)) break;
        
        size_t typeSavedPos = ts_.getPos();
        TypePtr t = parseType();
        if (ts_.getPos() == typeSavedPos) {
            errorAt(DiagCode::E2005, "expected return type");
            // Skip to closing parenthesis
            while (!ts_.isAtEnd() && !ts_.check(TokenType::RPAREN)) {
                ts_.advance();
            }
            break;
        }
        if (t && !t->isa<UnknownTypeAST>()) {
            types.push_back(std::move(t));
        }
    }
    
    ts_.consume(TokenType::RPAREN, "expected ')' to close return type list");
    
    auto builder = arena_.makeBuilder<TypePtr>();
    for (auto& t : types) builder.push_back(std::move(t));
    return builder.build();
}

// ============================================================================
// Qualifiers
// ============================================================================

QualifierSet Parser::parseQualifiers() {
    QualifierSet qs;
    auto& registry = QualifierRegistry::instance();
    
    while (ts_.check(TokenType::TILDE)) {
        SourceLocation loc = ts_.currentLoc();
        ts_.advance();
        
        if (!ts_.check(TokenType::IDENTIFIER)) {
            error(loc, DiagCode::E2003, "expected qualifier name after '~'");
            break;
        }
        InternedString name = pool_.intern(ts_.advance().value);
        
        const QualifierInfo* info = registry.lookup(name);
        // if (!info) {
        //     error(loc, DiagCode::E2010, 
        //           "unknown qualifier '~" + std::string(pool_.lookup(name)) + 
        //           "'; known qualifiers: " + registry.allNames());
        //     continue;
        // }
        
        qs.raw.push_back(name);
        qs.bitmask |= info->bit;
    }
    return qs;
}

// ============================================================================
// Module path parsing
// ============================================================================

std::vector<InternedString> Parser::parseModulePath() {
    std::vector<InternedString> path;
    if (!ts_.check(TokenType::IDENTIFIER)) return path;
    path.push_back(pool_.intern(ts_.advance().value));
    while (ts_.match(TokenType::DOT)) {
        if (!ts_.check(TokenType::IDENTIFIER)) {
            errorAt(DiagCode::E2003, "expected identifier after '.'");
            break;
        }
        path.push_back(pool_.intern(ts_.advance().value));
    }
    return path;
}