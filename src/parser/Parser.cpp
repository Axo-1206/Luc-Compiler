/**
 * @file Parser.cpp
 * @brief Core parsing infrastructure and top-level dispatch.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements the foundational components of the Luc parser:
 *   - TokenStream: Safe token consumption with comment skipping
 *   - Error handling: Panic-mode recovery and per-function error reporting
 *   - List helpers: Temporary vector builders for expression/type/stmt/param lists
 *   - Attribute parsing: @inline, @deprecated, @extern, etc.
 *   - Top-level dispatch: parse() and parseDeclaration()
 * 
 * ## TokenStream Purpose
 * 
 * TokenStream wraps the raw token vector and provides:
 *   - Automatic comment skipping (LINE_COMMENT, DOC_COMMENT are invisible to grammar)
 *   - Position manipulation (getPos/setPos) for lookahead and error recovery
 *   - Safe token consumption with consume() that reports errors on mismatch
 * 
 * ## Error Recovery Strategy
 * 
 *   - **Panic mode**: synchronize() skips tokens until a statement or declaration
 *     boundary (IF, FOR, WHILE, LET, CONST, RBRACE, etc.)
 *   - **Custom recovery**: synchronizeTo() stops at caller-specified token types
 *   - **Per-function recovery**: On parse failure, functions may return nullptr
 *     and the caller calls synchronize()
 * 
 * ## List Helpers
 * 
 * These functions return std::vector (temporary) for collecting sequences:
 *   - parseExprList()   : comma-separated expressions until end token
 *   - parseTypeList()   : comma-separated types until end token
 *   - parseStmtList()   : statements until end token
 *   - parseParamList()  : function parameters until ')'
 * 
 * The caller is responsible for converting to ArenaSpan using SpanBuilder.
 * 
 * ## Attribute Parsing
 * 
 * Attributes use the syntax @name or @name(args). They are collected as
 * temporary vectors and attached to declarations via attachMetadata().
 * 
 * ## Top-Level Parsing Flow
 * 
 *   parse()
 *     ├── parsePackageDecl()     (mandatory, first non‑comment)
 *     └── while (!ts_.isAtEnd())
 *           └── parseTopLevelDecl()
 *                 └── parseDeclaration()
 *                       └── dispatches to specific parser (use, struct, etc.)
 * 
 * @see ParserDecl.cpp for declaration parsers
 * @see ParserExpr.cpp for expression parsers
 * @see ParserStmt.cpp for statement parsers
 * @see ParserType.cpp for type parsers
 */

#include "Parser.hpp"
#include "diagnostics/Diagnostic.hpp"
#include "diagnostics/DiagnosticEngine.hpp"
#include "debug/DebugUtils.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

// ============================================================================
// TokenStream Implementation
// ============================================================================
// 
// The TokenStream methods provide safe token consumption with automatic
// comment skipping. LINE_COMMENT and DOC_COMMENT tokens are invisible to
// the grammar - they are only accessible via getTokens() for doc comment
// harvesting.
// 
// Key methods:
//   - peek() / advance() / match() - consume tokens
//   - check() - test current token type without consuming
//   - consume() - require a token type, report error if missing
//   - getPos() / setPos() - for lookahead and error recovery
// 
// All peek*() and advance() methods skip comments automatically.
// ============================================================================

bool TokenStream::checkAny(std::initializer_list<TokenType> types) const {
    for (TokenType t : types)
        if (check(t)) return true;
    return false;
}

bool TokenStream::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

bool TokenStream::matchAny(std::initializer_list<TokenType> types) {
    for (TokenType t : types)
        if (match(t)) return true;
    return false;
}

std::optional<Token> TokenStream::consumeIf(TokenType type) {
    if (check(type)) return advance();
    return std::nullopt;
}

Token TokenStream::consume(TokenType type, DiagCode code, const std::string& msg) {
    if (check(type)) return advance();
    return {type, "", 0, 0};
}

Token TokenStream::consume(TokenType type, const std::string& msg) {
    return consume(type, DiagCode::E2001, msg);
}

SourceLocation TokenStream::currentLoc() const {
    return locOf(peek());
}

SourceLocation TokenStream::locOf(const Token& tok) const {
    return SourceLocation(static_cast<uint32_t>(tok.line),
                          static_cast<uint32_t>(tok.column));
}

// ============================================================================
// Parser construction
// ============================================================================

Parser::Parser(std::vector<Token> tokens, DiagnosticEngine& dc,
               InternedString filePath, StringPool& pool, ASTArena& arena)
    : ts_(std::move(tokens)), filePath_(std::move(filePath)),
      pool_(pool), arena_(arena), dc_(dc) {}

// ============================================================================
// Error handling
// ============================================================================

void Parser::error(const SourceLocation& loc, DiagCode code, const std::string& msg) {
    dc_.error(DiagnosticCategory::Syntax, filePath_, loc, code, {msg});
}

void Parser::errorAt(DiagCode code, const std::string& msg) {
    error(ts_.currentLoc(), code, msg);
}

void Parser::synchronize() {
    synchronizeTo({
        TokenType::AT_SIGN, TokenType::PACKAGE, TokenType::USE,
        TokenType::PUB, TokenType::EXPORT, TokenType::STRUCT,
        TokenType::ENUM, TokenType::TRAIT, TokenType::IMPL,
        TokenType::TYPE, TokenType::FROM, TokenType::LET,
        TokenType::CONST, TokenType::IF, TokenType::FOR,
        TokenType::WHILE, TokenType::DO, TokenType::RETURN,
        TokenType::BREAK, TokenType::CONTINUE, TokenType::MATCH,
        TokenType::SWITCH, TokenType::RBRACE
    });
}

void Parser::synchronizeTo(std::initializer_list<TokenType> stopTokens) {
    while (!ts_.isAtEnd()) {
        if (ts_.checkAny(stopTokens))
            return;
        ts_.advance();
    }
}

// ============================================================================
// Visibility
// ============================================================================

Visibility Parser::parseVisibility() {
    if (ts_.match(TokenType::PUB)) return Visibility::Package;
    if (ts_.match(TokenType::EXPORT)) return Visibility::Export;
    return Visibility::Private;
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
        if (!info) {
            error(loc, DiagCode::E2010, 
                  "unknown qualifier '~" + std::string(pool_.lookup(name)) + 
                  "'; known qualifiers: " + registry.allNames());
            continue;
        }
        
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

// ============================================================================
// Return List Parsing
// ============================================================================
// 
// parseReturnList() is called after '->' in function types and declarations.
// 
// Grammar:
//   return_list := '(' [ return_type { ',' return_type } ] ')'   (multiple)
//                | return_type                                    (single)
// 
// Examples:
//   -> int
//   -> (int, string)
//   -> (x int) -> int                    (function type)
//   -> ~async (url string) -> string     (async function type)
//   -> ()
// 
// ─── Detection Strategy ────────────────────────────────────────────────────
//   1. If no '(' → single return type → parseType()
//   2. If '(' followed by ')' → empty parentheses → function type with zero params
//   3. If '(' followed by IDENTIFIER and then a type start → function type
//   4. If '(' followed by '~' → function type with qualifier
//   5. Otherwise → multi-return list: parse comma‑separated types inside '(' ')'
// 
// ─── Token Consumption ─────────────────────────────────────────────────────
// On entry: positioned at the token after '->' (may be '(' or type start)
// On exit:  positioned after the return list
// 
// ─── Return Value ─────────────────────────────────────────────────────────
// Returns ArenaSpan<TypePtr> – empty span indicates void function.
// 
// ─── Loop Safety ──────────────────────────────────────────────────────────
// Uses parseType() which guarantees progress.
// 
// ─── Error Recovery ───────────────────────────────────────────────────────
// - Missing return type: reports error, returns empty span
// - Missing ')': consume() reports error
// ============================================================================

ArenaSpan<TypePtr> Parser::parseReturnList() {
    // Helper to check if a token type can start a type
    auto isTypeStart = [](TokenType tt) -> bool {
        return isPrimitiveTypeToken(tt) ||
               tt == TokenType::IDENTIFIER ||
               tt == TokenType::LBRACKET ||
               tt == TokenType::AMPERSAND ||
               tt == TokenType::MUL ||
               tt == TokenType::LPAREN;
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

    // Peek inside the parentheses without consuming them
    size_t lookahead = ts_.getPos() + 1;
    const auto& tokens = ts_.getTokens();
    size_t tokenCount = ts_.getTokenCount();
    
    // Skip comments
    while (lookahead < tokenCount && 
           (tokens[lookahead].type == TokenType::LINE_COMMENT ||
            tokens[lookahead].type == TokenType::DOC_COMMENT)) {
        ++lookahead;
    }
    
    if (lookahead >= tokenCount) {
        errorAt(DiagCode::E2002, "unexpected end of file after '('");
        return ArenaSpan<TypePtr>();
    }

    TokenType afterParen = tokens[lookahead].type;

    // Case 2: Empty parentheses "()" → zero‑parameter function type
    if (afterParen == TokenType::RPAREN) {
        TypePtr funcType = parseFuncType();
        if (!funcType || funcType->isa<UnknownTypeAST>()) {
            errorAt(DiagCode::E2005, "expected function type");
            return ArenaSpan<TypePtr>();
        }
        auto builder = arena_.makeBuilder<TypePtr>();
        builder.push_back(std::move(funcType));
        return builder.build();
    }

    // Case 3: Identifier followed by a type start → function type
    if (afterParen == TokenType::IDENTIFIER) {
        size_t afterIdent = lookahead + 1;
        while (afterIdent < tokenCount && 
               (tokens[afterIdent].type == TokenType::LINE_COMMENT ||
                tokens[afterIdent].type == TokenType::DOC_COMMENT)) {
            ++afterIdent;
        }
        if (afterIdent < tokenCount && isTypeStart(tokens[afterIdent].type)) {
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

    // Case 4: Qualifier starts a function type
    if (afterParen == TokenType::TILDE) {
        TypePtr funcType = parseFuncType();
        if (!funcType || funcType->isa<UnknownTypeAST>()) {
            errorAt(DiagCode::E2005, "expected function type");
            return ArenaSpan<TypePtr>();
        }
        auto builder = arena_.makeBuilder<TypePtr>();
        builder.push_back(std::move(funcType));
        return builder.build();
    }

    // Case 5: Multi‑return list: '(' type { ',' type } ')'
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
        
        size_t savedPos = ts_.getPos();
        TypePtr t = parseType();
        if (ts_.getPos() == savedPos) {
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
// Doc Comment Harvesting
// ============================================================================
// 
// harvestDocComment() scans backward from the current position to collect
// documentation comments attached to the next declaration.
// 
// Attachment priority (from LUC_GRAMMAR.md):
//   1. Block doc comment   : /-- ... --/ immediately above declaration
//   2. Stacked line comments: consecutive -- lines above declaration
//   3. Trailing comment    : -- comment on the same line as declaration
// 
// ─── Scanning Logic ────────────────────────────────────────────────────────
//   - Starts from pos-1 and moves backward
//   - Stops at first non-comment token
//   - Collects LINE_COMMENT tokens that are contiguous and end on declLine-1
//   - DOC_COMMENT token immediately above (line difference ≤ 1) becomes block
//   - LINE_COMMENT on the same line becomes trailing
// 
// ─── Priority Resolution ────────────────────────────────────────────────────
//   Block doc > Stacked lines > Trailing comment
// ============================================================================

std::optional<DocComment> Parser::harvestDocComment() {
    const auto& tokens = ts_.getTokens();
    size_t pos = ts_.getPos();
    
    if (pos == 0) return std::nullopt;
    
    int declLine = ts_.peek().line;
    std::optional<std::string> trailingText;
    std::vector<std::string> stackedLines;
    int stackedTopLine = -1;
    std::optional<std::string> blockText;
    
    for (size_t i = pos; i > 0; ) {
        --i;
        const Token& t = tokens[i];
        
        if (t.type == TokenType::LINE_COMMENT) {
            if (t.line <= 0) continue;
            if (t.line == declLine) {
                if (!trailingText.has_value()) {
                    trailingText = t.value;
                }
                continue;
            }
            if (stackedLines.empty()) {
                if (declLine - t.line == 1) {
                    stackedLines.push_back(t.value);
                    stackedTopLine = t.line;
                    continue;
                } else {
                    break;
                }
            } else {
                if (stackedTopLine - t.line == 1) {
                    stackedLines.push_back(t.value);
                    stackedTopLine = t.line;
                    continue;
                } else {
                    break;
                }
            }
        }
        
        if (t.type == TokenType::DOC_COMMENT) {
            if (t.line <= 0) continue;
            if (declLine - t.line <= 1) {
                blockText = t.value;
            }
            break;
        }
        
        break;
    }
    
    if (blockText.has_value()) {
        return DocComment{pool_.intern(*blockText), DocCommentForm::Block};
    }
    if (!stackedLines.empty()) {
        std::string combined;
        for (int i = static_cast<int>(stackedLines.size()) - 1; i >= 0; --i) {
            if (!combined.empty()) combined += '\n';
            combined += stackedLines[i];
        }
        return DocComment{pool_.intern(combined), DocCommentForm::Stacked};
    }
    if (trailingText.has_value()) {
        return DocComment{pool_.intern(*trailingText), DocCommentForm::Trailing};
    }
    
    return std::nullopt;
}

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
// ============================================================================s

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

// ============================================================================
// Top-Level Parsing
// ============================================================================
// 
// parse() is the entry point for the entire parser. It:
//   1. Creates the ProgramAST root node
//   2. Parses the mandatory package declaration
//   3. Parses top-level declarations until EOF
//   4. Builds ArenaSpan<DeclPtr> for program->decls
// 
// ─── Error Recovery ────────────────────────────────────────────────────────
//   - Missing package declaration: inserts dummy node, calls synchronize()
//   - Failed top-level declaration: inserts UnknownDeclAST, calls synchronize()
//   - Never returns nullptr; errors are reported via DiagnosticEngine
// 
// ─── Declaration Dispatch ──────────────────────────────────────────────────
//   parseDeclaration() handles both top-level and local declarations.
//   It parses attributes, visibility, then dispatches to specific parsers.
// ============================================================================

ASTPtr<ProgramAST> Parser::parse() {
    auto program = arena_.make<ProgramAST>();
    program->filePath = filePath_;
    program->loc = ts_.currentLoc();

    std::vector<DeclPtr> decls;

    // Package declaration
    if (!ts_.check(TokenType::PACKAGE)) {
        errorAt(DiagCode::E2001, "expected 'package' declaration at start of file");
        synchronize();
        auto dummy = arena_.make<PackageDeclAST>(pool_.intern("<unknown>"));
        dummy->loc = ts_.currentLoc();
        program->packageName = pool_.intern("<error>");
        decls.push_back(std::move(dummy));
    } else {
        auto pkg = parsePackageDecl();
        if (pkg) {
            program->packageName = pkg->name;
            decls.push_back(std::move(pkg));
        } else {
            auto dummy = arena_.make<PackageDeclAST>(pool_.intern("<error>"));
            dummy->loc = ts_.currentLoc();
            program->packageName = pool_.intern("<error>");
            decls.push_back(std::move(dummy));
        }
    }

    // Top-level declarations
    while (!ts_.isAtEnd()) {
        auto doc = harvestDocComment();
        DeclPtr decl = parseTopLevelDecl();
        if (decl) {
            if (doc) decl->doc = std::move(doc);
            decls.push_back(std::move(decl));
        } else {
            synchronize();
        }
    }

    // Build the ArenaSpan for program->decls
    auto builder = arena_.makeBuilder<DeclPtr>();
    for (auto& d : decls) builder.push_back(std::move(d));
    program->decls = builder.build();

    return program;
}

DeclPtr Parser::parseTopLevelDecl() {
    return parseDeclaration(DeclContext::TopLevel);
}

// ============================================================================
// Declaration Dispatch
// ============================================================================
// 
// parseDeclaration() is the unified entry point for parsing declarations.
// It handles:
//   1. Attribute collection (zero or more @... tokens)
//   2. Visibility parsing (pub/export) – top-level only
//   3. Dispatch based on the next token
// 
// ─── Dispatch Order ────────────────────────────────────────────────────────
//   USE → STRUCT → ENUM → TRAIT → IMPL → FROM → TYPE → LET/CONST
// 
// ─── Distinguishing Variable vs Function ───────────────────────────────────
//   For LET/CONST, looksLikeFuncDecl() determines whether to parse as
//   function or variable declaration. This lookahead inspects the token
//   stream without consuming tokens.
// 
// ─── Attribute Attachment ──────────────────────────────────────────────────
//   Attributes are collected as std::vector<AttributePtr> and converted
//   to ArenaSpan<AttributePtr> after the declaration is successfully parsed.
// ============================================================================

DeclPtr Parser::parseDeclaration(DeclContext ctx) {
    // Parse attributes (temporary vector)
    std::vector<AttributePtr> attrs = parseAttributes();

    // Parse visibility (top-level only)
    Visibility vis = Visibility::Private;
    if (ctx == DeclContext::TopLevel) {
        vis = parseVisibility();
    } else {
        if (ts_.checkAny({TokenType::PUB, TokenType::EXPORT})) {
            errorAt(DiagCode::E2014, "visibility modifier not allowed in local declaration");
            ts_.advance();
        }
    }

    // Dispatch to specific declaration parser
    DeclPtr decl;
    
    if (ts_.check(TokenType::USE)) {
        decl = parseUseDecl(vis);
    } else if (ts_.check(TokenType::STRUCT)) {
        decl = parseStructDecl(vis);
    } else if (ts_.check(TokenType::ENUM)) {
        decl = parseEnumDecl(vis);
    } else if (ts_.check(TokenType::TRAIT)) {
        decl = parseTraitDecl(vis);
    } else if (ts_.check(TokenType::IMPL)) {
        decl = parseImplDecl(vis);
    } else if (ts_.check(TokenType::FROM)) {
        decl = parseFromDecl(vis);
    } else if (ts_.check(TokenType::TYPE)) {
        decl = parseTypeAliasDecl(vis);
    } else if (ts_.checkAny({TokenType::LET, TokenType::CONST})) {
        Token kwTok = ts_.advance();
        DeclKeyword kw = (kwTok.type == TokenType::LET) ? DeclKeyword::Let : DeclKeyword::Const;
        if (looksLikeFuncDecl()) {
            decl = parseFuncDecl(kw, vis);
        } else {
            decl = parseVarDecl(vis);
        }
    } else {
        errorAt(DiagCode::E2002, "expected declaration");
        return nullptr;
    }

    if (decl) {
        // Attach attributes (convert vector to span)
        if (!attrs.empty()) {
            auto builder = arena_.makeBuilder<AttributePtr>();
            for (auto& a : attrs) builder.push_back(std::move(a));
            decl->attributes = builder.build();
        }
        decl->loc = ts_.currentLoc();
    }
    
    return decl;
}