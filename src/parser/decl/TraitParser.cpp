#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

/**
 * @brief Parses a trait declaration (method contract, no implementations).
 * 
 * Grammar: `trait` IDENTIFIER [ `<` generic_params `>` ] `{` method* `}`
 * 
 * Example: `pub trait Drawable { draw (), bounds () -> Rect }`
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at 'trait' keyword
 * On exit:  positioned after the closing '}'
 * 
 * ─── Important Notes ───────────────────────────────────────────────────────
 *   - Trait methods are signatures only (no body, no '=')
 *   - Traits are top‑level only (semantic pass rejects local traits)
 *   - The semantic pass verifies impl blocks provide all methods
 * 
 * ─── Loop Safety ──────────────────────────────────────────────────────────
 * Uses saved position pattern with parseTraitMethod()
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing trait name: returns nullptr
 * - Missing '{' after name: reports error, returns nullptr
 * - Invalid method: skips method, continues
 * - Missing '}': consume() reports error
 */
ASTPtr<TraitDeclAST> Parser::parseTraitDecl(Visibility vis) {
    LUC_LOG_DECL_VERBOSE("parseTraitDecl: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::TRAIT, "expected 'trait'");

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_DECL("parseTraitDecl: ERROR - expected trait name");
        errorAt(DiagCode::E1003, "expected trait name");
        return nullptr;
    }
    InternedString name = pool_.intern(ts_.advance().value);
    LUC_LOG_DECL_EXTREME("parseTraitDecl: trait name = " << pool_.lookup(name));

    auto node = arena_.make<TraitDeclAST>();
    node->loc = loc;
    node->name = name;
    node->visibility = vis;

    if (ts_.check(TokenType::LESS)) {
        LUC_LOG_DECL_EXTREME("parseTraitDecl: parsing generic parameters");
        node->genericParams = parseGenericParams();
        LUC_LOG_DECL_EXTREME("parseTraitDecl: " << node->genericParams.size() << " generic parameter(s)");
    }

    ts_.consume(TokenType::LBRACE, "expected '{' to open trait body");

    std::vector<TraitMethodPtr> methods;
    int methodCount = 0;
    
    while (!ts_.check(TokenType::RBRACE) && !ts_.isAtEnd()) {
        ts_.match(TokenType::SEMICOLON);
        ts_.match(TokenType::COMMA);
        if (ts_.check(TokenType::RBRACE)) break;

        size_t savedPos = ts_.getPos();
        TraitMethodPtr method = parseTraitMethod();
        if (method) {
            methodCount++;
            LUC_LOG_DECL_EXTREME("parseTraitDecl: parsed method #" << methodCount);
            methods.push_back(std::move(method));
        } else {
            LUC_LOG_DECL("parseTraitDecl: ERROR - failed to parse trait method");
            if (ts_.getPos() == savedPos && !ts_.isAtEnd()) ts_.advance();
            while (!ts_.isAtEnd() && !ts_.check(TokenType::RBRACE) && 
                   !ts_.check(TokenType::IDENTIFIER)) ts_.advance();
        }
    }

    auto builder = arena_.makeBuilder<TraitMethodPtr>();
    for (auto& m : methods) builder.push_back(std::move(m));
    node->methods = builder.build();

    ts_.consume(TokenType::RBRACE, "expected '}' to close trait body");
    
    LUC_LOG_DECL_VERBOSE("parseTraitDecl: parsed " << methodCount << " method(s)");
    return node;
}

// Add logging to parseTraitMethod and parseTraitRef as well
TraitMethodPtr Parser::parseTraitMethod() {
    LUC_LOG_DECL_EXTREME("parseTraitMethod: entering");
    SourceLocation loc = ts_.currentLoc();
    
    auto method = arena_.make<TraitMethodAST>();
    method->loc = loc;

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_DECL("parseTraitMethod: ERROR - expected trait method name");
        errorAt(DiagCode::E1003, "expected trait method name");
        return nullptr;
    }
    method->name = pool_.intern(ts_.advance().value);
    LUC_LOG_DECL_EXTREME("parseTraitMethod: method name = " << pool_.lookup(method->name));
    
    // ... rest of existing code ...
    
    LUC_LOG_DECL_EXTREME("parseTraitMethod: success");
    return method;
}

TraitRefPtr Parser::parseTraitRef() {
    LUC_LOG_DECL_EXTREME("parseTraitRef: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::COLON, "expected ':' before trait name");

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_DECL("parseTraitRef: ERROR - expected trait name after ':'");
        errorAt(DiagCode::E1003, "expected trait name after ':'");
        return nullptr;
    }

    auto ref = arena_.make<TraitRefAST>();
    ref->loc = loc;
    ref->name = pool_.intern(ts_.advance().value);
    LUC_LOG_DECL_EXTREME("parseTraitRef: trait name = " << pool_.lookup(ref->name));

    if (ts_.check(TokenType::LESS)) {
        LUC_LOG_DECL_EXTREME("parseTraitRef: parsing generic arguments");
        ref->genericArgs = parseGenericArgs();
        LUC_LOG_DECL_EXTREME("parseTraitRef: " << ref->genericArgs.size() << " generic argument(s)");
    }

    return ref;
}