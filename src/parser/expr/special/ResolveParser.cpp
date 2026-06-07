#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

/**
 * @brief Parses a `resolve` expression for structured error handling.
 * 
 * Grammar:
 *   resolve_expr := 'resolve' expr '{' ok_arm err_arm '}'
 * 
 * Example:
 *   resolve divide(10, 0) {
 *       ok  (v int)    { return v  }
 *       err (e string) { return -1 }
 *   }
 * 
 * ─── Semantics ─────────────────────────────────────────────────────────────
 *   - The `subject` must evaluate to a `T!E` type
 *   - The `ok` arm receives the unwrapped success value (plain T)
 *   - The `err` arm receives the error value (type E, or nothing for bare '!')
 *   - Both arms are required and must return the same type
 *   - After the `resolve` block, the `!` is consumed; the expression result
 *     is plain T (the type returned by the arms)
 * 
 * ─── Comparison with `??` ──────────────────────────────────────────────────
 *   `??` provides a fallback value and discards the error.
 *   `resolve` allows inspection of the error and full control flow.
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at 'resolve' keyword
 * On exit:  positioned after the closing '}'
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing subject: returns UnknownExprAST
 * - Missing '{' after subject: reports error, returns UnknownExprAST
 * - Missing 'ok' arm: reports error, continues with null okArm
 * - Missing 'err' arm: reports error, continues with null errArm
 * - Missing '}': consume() reports error, returns node with whatever was parsed
 * 
 * @return ExprPtr – ResolveExprAST on success, UnknownExprAST on error
 */
ExprPtr Parser::parseResolveExpr() {
    LUC_LOG_EXPR_VERBOSE("parseResolveExpr: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::RESOLVE, "expected 'resolve'");

    ExprPtr subject = parseExpr(false);
    if (!subject) {
        LUC_LOG_EXPR("parseResolveExpr: ERROR - expected expression after 'resolve'");
        errorAt(DiagCode::E1008, "expected expression after 'resolve'");
        return arena_.make<UnknownExprAST>();
    }
    LUC_LOG_EXPR_EXTREME("parseResolveExpr: subject parsed");

    ts_.consume(TokenType::LBRACE, "expected '{' after resolve subject");

    OkArmPtr okArm = parseOkArm();
    if (!okArm) {
        LUC_LOG_EXPR("parseResolveExpr: ERROR - expected 'ok' arm");
        errorAt(DiagCode::E1006, "expected 'ok' arm in resolve expression");
    } else {
        LUC_LOG_EXPR_EXTREME("parseResolveExpr: ok arm parsed");
    }

    ErrArmPtr errArm = parseErrArm();
    if (!errArm) {
        LUC_LOG_EXPR("parseResolveExpr: ERROR - expected 'err' arm");
        errorAt(DiagCode::E1006, "expected 'err' arm in resolve expression");
    } else {
        LUC_LOG_EXPR_EXTREME("parseResolveExpr: err arm parsed");
    }

    ts_.consume(TokenType::RBRACE, "expected '}' to close resolve expression");

    auto node = arena_.make<ResolveExprAST>();
    node->loc = loc;
    node->subject = subject;
    node->okArm = okArm;
    node->errArm = errArm;
    
    LUC_LOG_EXPR_VERBOSE("parseResolveExpr: success");
    return node;
}

/**
 * @brief Parses the `ok` arm of a `resolve` expression.
 * 
 * Grammar:
 *   ok_arm := 'ok' '(' IDENTIFIER type ')' block
 * 
 * Example:
 *   ok (v int) { return v * 2 }
 * 
 * ─── Semantics ─────────────────────────────────────────────────────────────
 *   - The identifier is bound to the unwrapped success value
 *   - The type annotation is required and must be the success type T
 *   - The block is executed when the subject succeeded
 *   - The block's return type determines the type of the entire `resolve`
 *     expression
 * 
 * ─── Important Restriction ─────────────────────────────────────────────────
 *   The `bindType` is always plain T (never T!E). The `!` is consumed at the
 *   resolve boundary before the arm is entered.
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at 'ok' keyword
 * On exit:  positioned after the closing '}' of the block
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing '(' after 'ok': reports error, returns nullptr
 * - Missing identifier: reports error, returns nullptr
 * - Missing type after identifier: reports error, returns nullptr
 * - Missing ')' after type: reports error, returns nullptr
 * - Missing block: reports error, returns arm with null body
 * 
 * @return OkArmPtr – OkArmAST on success, nullptr on error
 */
OkArmPtr Parser::parseOkArm() {
    LUC_LOG_EXPR_EXTREME("parseOkArm: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::OK, "expected 'ok'");
    ts_.consume(TokenType::LPAREN, "expected '(' after 'ok'");

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_EXPR("parseOkArm: ERROR - expected identifier");
        errorAt(DiagCode::E1003, "expected identifier for ok arm binding");
        return nullptr;
    }
    InternedString bindName = pool_.intern(ts_.advance().value);
    LUC_LOG_EXPR_EXTREME("parseOkArm: bindName = " << pool_.lookup(bindName));

    TypePtr bindType = parseType();
    if (!bindType) {
        LUC_LOG_EXPR("parseOkArm: ERROR - expected type");
        errorAt(DiagCode::E1005, "expected type for ok arm binding");
        return nullptr;
    }

    ts_.consume(TokenType::RPAREN, "expected ')' after ok arm type");

    StmtPtr body = parseBlock();
    if (!body) {
        LUC_LOG_EXPR("parseOkArm: ERROR - expected block");
        errorAt(DiagCode::E1001, "expected block for ok arm");
        return nullptr;
    }

    auto arm = arena_.make<OkArmAST>();
    arm->loc = loc;
    arm->bindName = bindName;
    arm->bindType = bindType;
    arm->body = body;
    
    LUC_LOG_EXPR_EXTREME("parseOkArm: success");
    return arm;
}

/**
 * @brief Parses the `err` arm of a `resolve` expression.
 * 
 * Grammar:
 *   err_arm := 'err' '(' [ IDENTIFIER type ] ')' block
 * 
 * Examples:
 *   err (e string) { return -1 }    -- typed error
 *   err () { return 0 }             -- bare '!' (no error payload)
 * 
 * ─── Semantics ─────────────────────────────────────────────────────────────
 *   - For typed errors (E): the identifier is bound to the error value
 *   - For bare '!': the parentheses are empty; no binding is introduced
 *   - The block is executed when the subject failed
 *   - The block's return type must match the `ok` arm's return type
 * 
 * ─── Detection ─────────────────────────────────────────────────────────────
 *   The presence of an identifier before the ')' determines whether this is a
 *   typed error or a bare '!':
 *     - err (e string) → typed error (bindName = "e", bindType = string)
 *     - err ()         → bare '!' (bindName = "", bindType = nullptr)
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at 'err' keyword
 * On exit:  positioned after the closing '}' of the block
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing '(' after 'err': reports error, returns nullptr
 * - If identifier present but type missing: reports error, returns nullptr
 * - Missing ')' after (optional) type: reports error, returns nullptr
 * - Missing block: reports error, returns arm with null body
 * 
 * @return ErrArmPtr – ErrArmAST on success, nullptr on error
 */
ErrArmPtr Parser::parseErrArm() {
    LUC_LOG_EXPR_EXTREME("parseErrArm: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::ERR, "expected 'err'");
    ts_.consume(TokenType::LPAREN, "expected '(' after 'err'");

    InternedString bindName;
    TypePtr bindType = nullptr;
    bool isBare = false;

    // Check for optional identifier and type (bare '!' case)
    if (!ts_.check(TokenType::RPAREN)) {
        if (!ts_.check(TokenType::IDENTIFIER)) {
            LUC_LOG_EXPR("parseErrArm: ERROR - expected identifier");
            errorAt(DiagCode::E1003, "expected identifier for err arm binding");
        } else {
            bindName = pool_.intern(ts_.advance().value);
            LUC_LOG_EXPR_EXTREME("parseErrArm: bindName = " << pool_.lookup(bindName));
            bindType = parseType();
            if (!bindType) {
                LUC_LOG_EXPR("parseErrArm: ERROR - expected type");
                errorAt(DiagCode::E1005, "expected type for err arm binding");
            }
        }
    } else {
        isBare = true;
        LUC_LOG_EXPR_EXTREME("parseErrArm: bare '!' (no error payload)");
    }

    ts_.consume(TokenType::RPAREN, "expected ')' after err arm");

    StmtPtr body = parseBlock();
    if (!body) {
        LUC_LOG_EXPR("parseErrArm: ERROR - expected block");
        errorAt(DiagCode::E1001, "expected block for err arm");
        return nullptr;
    }

    auto arm = arena_.make<ErrArmAST>();
    arm->loc = loc;
    arm->bindName = bindName;
    arm->bindType = bindType;
    arm->body = body;
    
    LUC_LOG_EXPR_EXTREME("parseErrArm: success" << (isBare ? " (bare)" : ""));
    return arm;
}