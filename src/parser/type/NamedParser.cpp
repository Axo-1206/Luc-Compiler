#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Named Type
// ============================================================================
// 
// parseNamedType() parses a user-defined type reference with optional generic
// arguments.
// 
// Grammar: IDENTIFIER [ '<' type { ',' type } '>' ]
// 
// Examples:
//   Vec2
//   Buffer<int>
//   Map<string, Vec2>
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at IDENTIFIER (type name)
// On exit:  positioned after generic arguments (or after name if none)
// 
// ─── Generic Arguments ────────────────────────────────────────────────────
//   - Parsed via parseGenericArgs() (consumes '<' ... '>')
//   - Empty list `<` `>` is allowed
//   - Semantic pass validates argument count against declaration
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
// - Missing type name: returns UnknownTypeAST
// - Generic argument errors: reported by parseGenericArgs()
// ============================================================================

TypePtr Parser::parseNamedType() {
    LUC_LOG_TYPE_EXTREME("parseNamedType: entering");
    SourceLocation loc = ts_.currentLoc();
    
    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_TYPE("parseNamedType: ERROR - expected type name, got '" << ts_.peek().value << "'");
        errorAt(DiagCode::E1003, "expected type name");
        return arena_.make<UnknownTypeAST>();
    }
    
    InternedString name = pool_.intern(ts_.advance().value);
    LUC_LOG_TYPE_EXTREME("parseNamedType: name = " << pool_.lookup(name));
    
    auto node = arena_.make<NamedTypeAST>(name);
    node->loc = loc;

    if (ts_.check(TokenType::LESS)) {
        LUC_LOG_TYPE_EXTREME("parseNamedType: parsing generic arguments");
        node->genericArgs = parseGenericArgs();
        LUC_LOG_TYPE_EXTREME("parseNamedType: found " << node->genericArgs.size() << " generic argument(s)");
    }

    return node;
}