#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Attribute Parsing
// ============================================================================
// 
// Attributes use the syntax: @name or @name(arg1, arg2, ...)
// 
// Supported arguments (from LUC_GRAMMAR.md):
//   - String literals     : "hello"
//   - Integer literals    : 42, 0xFF, 0b1010
//   - Boolean literals    : true, false
//   - Type identifiers    : TypeName (e.g., in @extern("malloc", C))
// 
// Attributes are collected as temporary vectors and attached to declarations
// via attachMetadata() in parseDeclaration(). The semantic pass validates
// attribute names and arguments.
// 
// Example:
//   @inline
//   @deprecated("Use newAPI")
//   @extern("malloc") const malloc (size uint64) -> *uint8?
// ============================================================================

std::vector<AttributePtr> Parser::parseAttributes() {
    std::vector<AttributePtr> attrs;
    while (ts_.check(TokenType::AT_SIGN)) {
        AttributePtr attr = parseAttribute();
        if (attr) attrs.push_back(std::move(attr));
    }
    return attrs;
}

AttributePtr Parser::parseAttribute() {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::AT_SIGN, "expected '@'");
    
    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected attribute name after '@'");
        return nullptr;
    }
    InternedString name = pool_.intern(ts_.advance().value);
    
    auto attr = arena_.make<AttributeAST>();
    attr->name = name;
    attr->loc = loc;

    if (ts_.match(TokenType::LPAREN)) {
        std::vector<AttributeArgPtr> args;
        while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
            if (!args.empty() && !ts_.match(TokenType::COMMA))
                break;
            AttributeArgPtr arg = parseAttributeArgLiteral();
            if (arg) args.push_back(std::move(arg));
        }
        ts_.consume(TokenType::RPAREN, "expected ')' after attribute arguments");
        
        auto builder = arena_.makeBuilder<AttributeArgPtr>();
        for (auto& a : args) builder.push_back(std::move(a));
        attr->args = builder.build();
    }
    return attr;
}

AttributeArgPtr Parser::parseAttributeArgLiteral() {
    if (ts_.check(TokenType::STRING_LITERAL)) {
        return arena_.make<AttributeArgAST>(AttributeArgKind::StringLit,
                                            pool_.intern(ts_.advance().value));
    }
    if (ts_.check(TokenType::INT_LITERAL) || ts_.check(TokenType::HEX_LITERAL) ||
        ts_.check(TokenType::BINARY_LITERAL)) {
        return arena_.make<AttributeArgAST>(AttributeArgKind::IntLit,
                                            pool_.intern(ts_.advance().value));
    }
    if (ts_.check(TokenType::TRUE) || ts_.check(TokenType::FALSE)) {
        return arena_.make<AttributeArgAST>(AttributeArgKind::BoolLit,
                                            pool_.intern(ts_.advance().value));
    }
    if (ts_.check(TokenType::IDENTIFIER)) {
        return arena_.make<AttributeArgAST>(AttributeArgKind::TypeIdent,
                                            pool_.intern(ts_.advance().value));
    }
    errorAt(DiagCode::E2002, "expected string, integer, boolean, or type identifier in attribute argument");
    return nullptr;
}