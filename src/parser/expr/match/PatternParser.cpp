#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"

// ============================================================================
// Pattern Dispatcher
// ============================================================================
// 
// parsePattern() is the root dispatcher for parsing a single pattern.
// 
// Dispatch Priority:
//   1. '_' → parseWildcardPattern()
//   2. Literal tokens (INT, FLOAT, STRING, etc.) → parseLiteralOrRangePattern()
//   3. IDENTIFIER:
//        a. IDENTIFIER 'is' type → parseTypePattern()
//        b. IDENTIFIER '{' → parseStructPattern()
//        c. IDENTIFIER '.' → Qualified constant → parseExpr() + PatternExprAST
//        d. Otherwise → parseBindPattern()
//   4. No match → error
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at the first token of a pattern
// On exit:  positioned after the pattern
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
//   - No pattern recognised: reports error, returns nullptr
//   - Bind pattern followed by '..': error, recovers by consuming range
// ============================================================================

ASTPtr<PatternAST> Parser::parsePattern() {
    // Wildcard
    if (ts_.check(TokenType::WILDCARD)) {
        return parseWildcardPattern();
    }

    // Literal patterns (and ranges)
    switch (ts_.peekType()) {
        case TokenType::INT_LITERAL:
        case TokenType::FLOAT_LITERAL:
        case TokenType::STRING_LITERAL:
        case TokenType::RAW_STRING_LITERAL:
        case TokenType::CHAR_LITERAL:
        case TokenType::HEX_LITERAL:
        case TokenType::BINARY_LITERAL:
        case TokenType::TRUE:
        case TokenType::FALSE:
        case TokenType::NIL:
        case TokenType::MINUS:
            return parseLiteralOrRangePattern();
        default:
            break;
    }

    // IDENTIFIER-based patterns
    if (ts_.check(TokenType::IDENTIFIER)) {
        std::string name = ts_.peek().value;

        // Type pattern: IDENTIFIER 'is' type
        if (ts_.peekNextType() == TokenType::IS) {
            ts_.advance(); // consume IDENTIFIER
            return parseTypePattern(pool_.intern(name));
        }

        // Struct pattern: IDENTIFIER '{'
        if (ts_.peekNextType() == TokenType::LBRACE) {
            ts_.advance(); // consume IDENTIFIER
            return parseStructPattern(pool_.intern(name));
        }

        // Qualified constant pattern: IDENTIFIER '.' ...
        if (ts_.peekNextType() == TokenType::DOT) {
            ExprPtr expr = parseExpr();
            if (!expr) {
                errorAt(DiagCode::E2007, "expected expression after '.' in pattern");
                return nullptr;
            }
            return arena_.make<PatternExprAST>(std::move(expr));
        }

        // Simple bind pattern
        ts_.advance(); // consume IDENTIFIER
        if (ts_.check(TokenType::RANGE)) {
            errorAt(DiagCode::E2007, "bind patterns cannot be used as range bounds");
            ts_.advance(); // consume '..'
            parseLiteralOrRangePattern(); // recover
        }
        return parseBindPattern(pool_.intern(name));
    }

    errorAt(DiagCode::E2007, "expected pattern");
    return nullptr;
}

// ============================================================================
// Literal or Range Pattern
// ============================================================================
// 
// parseLiteralOrRangePattern() parses a literal pattern or a range pattern.
// 
// Grammar:
//   literal_pattern := literal
//   range_pattern   := literal '..' [ '<' ] literal
// 
// Examples:
//   42                 → literal pattern
//   "ok"               → literal pattern
//   1..10              → inclusive range pattern
//   1..<10             → exclusive range pattern
//   -5..5              → negative literal as lower bound
// 
// ─── Supported Literal Types ──────────────────────────────────────────────
//   INT_LITERAL, FLOAT_LITERAL, STRING_LITERAL, RAW_STRING_LITERAL,
//   CHAR_LITERAL, HEX_LITERAL, BINARY_LITERAL, TRUE, FALSE, NIL
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at literal token (or '-' for negative)
// On exit:  positioned after the literal (or after hi literal for range)
// 
// ─── Negative Literal Handling ────────────────────────────────────────────
//   - Unary minus is consumed and applied to the literal value
//   - The raw value is stored as "-42" in the LiteralExprAST
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
//   - Invalid literal: returns nullptr
//   - Missing hi after '..': reports error, returns nullptr
// ============================================================================

ASTPtr<PatternAST> Parser::parseLiteralOrRangePattern() {
    SourceLocation loc = ts_.currentLoc();

    // Handle unary minus for negative literals
    bool negative = false;
    if (ts_.check(TokenType::MINUS)) {
        negative = true;
        ts_.advance();
    }

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
            errorAt(DiagCode::E2009, "expected literal value in pattern");
            return nullptr;
    }

    std::string rawValue = negative ? ("-" + tok.value) : tok.value;
    InternedString internedValue = pool_.intern(rawValue);

    // Check for range: lo '..' [ '<' ] hi
    if (ts_.check(TokenType::RANGE)) {
        ts_.advance(); // consume '..'
        bool isExclusive = ts_.match(TokenType::LESS);

        bool negHi = false;
        if (ts_.check(TokenType::MINUS)) {
            negHi = true;
            ts_.advance();
        }

        if (!ts_.checkAny({TokenType::INT_LITERAL, TokenType::HEX_LITERAL, TokenType::FLOAT_LITERAL})) {
            errorAt(DiagCode::E2009, "expected literal after '..' in range pattern");
            return nullptr;
        }
        Token hiTok = ts_.advance();
        std::string hiRaw = negHi ? ("-" + hiTok.value) : hiTok.value;
        InternedString hiInterned = pool_.intern(hiRaw);

        LiteralKind hiKind;
        switch (hiTok.type) {
            case TokenType::INT_LITERAL: hiKind = LiteralKind::Int; break;
            case TokenType::HEX_LITERAL: hiKind = LiteralKind::Hex; break;
            default: hiKind = LiteralKind::Float; break;
        }

        auto loExpr = arena_.make<LiteralExprAST>(kind, internedValue);
        loExpr->loc = loc;
        auto hiExpr = arena_.make<LiteralExprAST>(hiKind, hiInterned);
        hiExpr->loc = ts_.locOf(hiTok);

        auto range = arena_.make<RangeExprAST>();
        range->loc = loc;
        range->lo = std::move(loExpr);
        range->hi = std::move(hiExpr);
        range->isExclusive = isExclusive;

        return arena_.make<PatternExprAST>(std::move(range));
    }

    // Simple literal pattern
    auto lit = arena_.make<LiteralExprAST>(kind, internedValue);
    lit->loc = loc;
    return arena_.make<PatternExprAST>(std::move(lit));
}

// ============================================================================
// Bind Pattern
// ============================================================================
// 
// parseBindPattern() parses an identifier pattern that binds the matched value.
// 
// Grammar: IDENTIFIER
// 
// Example: `n` → binds matched value to variable 'n'
// 
// ─── Scope Introduction ────────────────────────────────────────────────────
//   - Introduces a new variable in the arm's scope
//   - Variable type is the type of the matched value (narrowed by pattern)
//   - Accessible in guard expression and arm body
// 
// ─── Preconditions ────────────────────────────────────────────────────────
//   - Called after the IDENTIFIER token has been consumed
//   - The name is passed as a parameter (already interned)
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
//   - Consumes no additional tokens (identifier already consumed by caller)
// ============================================================================

ASTPtr<BindPatternAST> Parser::parseBindPattern(InternedString name) {
    SourceLocation loc = ts_.currentLoc();
    auto pat = arena_.make<BindPatternAST>(name);
    pat->loc = loc;
    return pat;
}

// ============================================================================
// Type Pattern
// ============================================================================
// 
// parseTypePattern() parses a pattern that combines type check with binding.
// 
// Grammar: IDENTIFIER 'is' type
// 
// Example: `s is Circle` → matches if subject is Circle, binds as 's' typed Circle
// 
// ─── Semantics ────────────────────────────────────────────────────────────
//   - Runtime type check: subject must match `checkType`
//   - If match succeeds, value is bound to `bindName` with narrowed type
//   - The bound variable's type is `checkType` in the arm body
// 
// ─── Preconditions ────────────────────────────────────────────────────────
//   - Called after the IDENTIFIER has been consumed
//   - The bindName is passed as a parameter (already interned)
//   - Current token is 'is'
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
//   - Consumes 'is' keyword
//   - Parses the type annotation via parseType()
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
//   - Missing 'is': consume() reports error, returns nullptr
//   - Missing or invalid type: reports error, returns nullptr
// ============================================================================

ASTPtr<TypePatternAST> Parser::parseTypePattern(InternedString bindName) {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::IS, "expected 'is' in type pattern");

    TypePtr checkType = parseType();
    if (!checkType) {
        errorAt(DiagCode::E2005, "expected type after 'is' in type pattern");
        return nullptr;
    }

    auto pat = arena_.make<TypePatternAST>();
    pat->loc = loc;
    pat->bindName = bindName;
    pat->checkType = std::move(checkType);
    return pat;
}

// ============================================================================
// Wildcard Pattern
// ============================================================================
// 
// parseWildcardPattern() parses the '_' pattern that matches any value.
// 
// Grammar: '_'
// 
// Example: `_ => "anything"`
// 
// ─── Semantics ────────────────────────────────────────────────────────────
//   - Matches any value (like a bind pattern)
//   - Does NOT introduce a variable name into the arm's scope
//   - The matched value is discarded and cannot be referenced
// 
// ─── Distinction from 'default' ───────────────────────────────────────────
//   - '_' is a pattern that may appear in any arm position
//   - 'default' is a keyword for the required fallback arm (not a pattern)
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
//   - Consumes the '_' token
// ============================================================================

ASTPtr<WildcardPatternAST> Parser::parseWildcardPattern() {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::WILDCARD, "expected '_'");
    auto pat = arena_.make<WildcardPatternAST>();
    pat->loc = loc;
    return pat;
}

// ============================================================================
// Struct Pattern
// ============================================================================
// 
// parseStructPattern() parses a struct destructuring pattern.
// 
// Grammar: IDENTIFIER '{' { field_pattern } '}'
// 
// Examples:
//   Vec2 { x, y }                     → shorthand: binds x and y from subject
//   Vec2 { x: 0.0, y: 0.0 }          → exact match on field values
//   Player { health: 0, name }       → mixed: exact match on health, bind name
//   Vec2 { x: 0.0, y: v }            → nested: bind y's value to variable v
// 
// ─── Semantics ────────────────────────────────────────────────────────────
//   - Matches when subject is a struct of the named type
//   - Fields not listed are ignored (match succeeds regardless)
//   - For each field pattern:
//        * Shorthand (no ':'): binds field's value to variable with same name
//        * Full form (':' pattern): field value must match the sub‑pattern
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned after the type name (at '{')
// On exit:  positioned after the closing '}'
// 
// ─── Loop Safety ──────────────────────────────────────────────────────────
//   - Uses saved position pattern with parseFieldPattern()
//   - If parseFieldPattern() makes no progress, consumes one token and continues
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
//   - Missing '{': consume() reports error, returns nullptr
//   - Invalid field pattern: skips field, continues parsing remaining fields
//   - Missing '}': consume() reports error
// ============================================================================

ASTPtr<StructPatternAST> Parser::parseStructPattern(InternedString typeName) {
    SourceLocation loc = ts_.currentLoc();
    ts_.consume(TokenType::LBRACE, "expected '{' in struct pattern");

    auto pat = arena_.make<StructPatternAST>();
    pat->loc = loc;
    pat->typeName = typeName;

    std::vector<FieldPatternPtr> fields;

    while (!ts_.check(TokenType::RBRACE) && !ts_.isAtEnd()) {
        ts_.match(TokenType::COMMA);
        if (ts_.check(TokenType::RBRACE)) break;

        size_t savedPos = ts_.getPos();
        FieldPatternPtr fp = parseFieldPattern();
        if (fp) {
            fields.push_back(std::move(fp));
        } else {
            if (ts_.getPos() == savedPos && !ts_.isAtEnd()) {
                errorAt(DiagCode::E2003, "expected field name in struct pattern");
                ts_.advance();
            }
        }
    }

    auto builder = arena_.makeBuilder<FieldPatternPtr>();
    for (auto& f : fields) builder.push_back(std::move(f));
    pat->fields = builder.build();

    ts_.consume(TokenType::RBRACE, "expected '}' to close struct pattern");
    return pat;
}

// ============================================================================
// Field Pattern
// ============================================================================
// 
// parseFieldPattern() parses a single field entry inside a struct pattern.
// 
// Grammar: IDENTIFIER [ ':' pattern ]
// 
// Examples:
//   x               → shorthand: bind field 'x' to variable 'x'
//   x: 0.0          → full form: match field 'x' against literal 0.0
//   pos: Vec2 { ... } → nested: match field 'pos' against a struct pattern
// 
// ─── Semantics ────────────────────────────────────────────────────────────
//   - Shorthand form: equivalent to field_name: bind_pattern(field_name)
//   - Full form: field's value must match the given sub‑pattern
//   - Sub‑pattern can be any valid pattern (literal, range, bind, type, struct)
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at field name
// On exit:  positioned after sub‑pattern (or after field name if no sub‑pattern)
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
//   - Missing field name: returns nullptr
//   - Missing sub‑pattern after ':': reports error (field node still created)
// ============================================================================

FieldPatternPtr Parser::parseFieldPattern() {
    SourceLocation loc = ts_.currentLoc();

    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected field name in struct pattern");
        return nullptr;
    }
    InternedString fieldName = pool_.intern(ts_.advance().value);

    auto fp = arena_.make<FieldPatternAST>();
    fp->loc = loc;
    fp->field = fieldName;

    // Full form: 'fieldName : sub_pattern'
    if (ts_.check(TokenType::COLON)) {
        ts_.advance(); // consume ':'
        fp->subPattern = parsePattern();
        if (!fp->subPattern) {
            errorAt(DiagCode::E2007, "expected sub-pattern after ':' in field pattern");
        }
    }
    // else: shorthand — subPattern is nullptr, bind by field name

    return fp;
}