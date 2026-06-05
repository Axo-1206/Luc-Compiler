#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

/**
 * @brief Parses a type alias declaration.
 * 
 * Grammar: `type` IDENTIFIER [ `<` generic_params `>` ] `=` type
 * 
 * Example: `type Transform<T> = (v T) -> T`
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at 'type' keyword
 * On exit:  positioned after the aliased type
 * 
 * ─── Important Notes ───────────────────────────────────────────────────────
 *   - Does NOT create a new nominal type (unlike struct)
 *   - Alias is interchangeable with its target
 *   - Generic parameters allow instantiation with concrete types
 *   - Can be top‑level or local
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing alias name: returns nullptr
 * - Missing '=' after name/generics: reports error, returns nullptr
 * - Missing aliased type: reports error (node created with null aliasedType)
 */
ASTPtr<TypeAliasDeclAST> Parser::parseTypeAliasDecl(Visibility vis) {
    LUC_LOG_DECL_VERBOSE("parseTypeAliasDecl: entering");
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::TYPE, "expected 'type' before type alias");

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_DECL("parseTypeAliasDecl: ERROR - expected type alias name");
        errorAt(DiagCode::E1003, "expected type alias name");
        return nullptr;
    }
    InternedString name = pool_.intern(ts_.advance().value);
    LUC_LOG_DECL_EXTREME("parseTypeAliasDecl: alias name = " << pool_.lookup(name));

    auto node = arena_.make<TypeAliasDeclAST>();
    node->loc = loc;
    node->name = name;
    node->visibility = vis;

    if (ts_.check(TokenType::LESS)) {
        LUC_LOG_DECL_EXTREME("parseTypeAliasDecl: parsing generic parameters");
        node->genericParams = parseGenericParams();
        LUC_LOG_DECL_EXTREME("parseTypeAliasDecl: " << node->genericParams.size() << " generic parameter(s)");
    }

    ts_.consume(TokenType::ASSIGN, "expected '=' in type alias");

    node->aliasedType = parseType();
    if (!node->aliasedType) {
        LUC_LOG_DECL("parseTypeAliasDecl: ERROR - expected type on right-hand side");
        errorAt(DiagCode::E1005, "expected type on the right-hand side of type alias");
    } else {
        LUC_LOG_DECL_EXTREME("parseTypeAliasDecl: aliased type parsed");
    }

    LUC_LOG_DECL_VERBOSE("parseTypeAliasDecl: success");
    return node;
}