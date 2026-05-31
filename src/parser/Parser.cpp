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
#include "diagnostics/Diagnostic.hpp"   // new diagnostic API
#include "debug/DebugUtils.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

// ============================================================================
// TokenStream Implementation
// ============================================================================

const Token TokenStream::eofToken = {TokenType::EOF_TOKEN, "", 0, 0};

TokenStream::TokenStream(std::vector<Token> tokens, InternedString filePath)
    : tokens_(std::move(tokens)), filePath_(filePath) {}

size_t TokenStream::skipCommentsFrom(size_t start) const {
    size_t i = start;
    while (i < tokens_.size() &&
           (tokens_[i].type == TokenType::LINE_COMMENT ||
            tokens_[i].type == TokenType::DOC_COMMENT)) {
        ++i;
    }
    return i;
}

bool TokenStream::isAtEnd() const {
    return skipCommentsFrom(pos_) >= tokens_.size();
}

const Token& TokenStream::peek() const {
    size_t idx = skipCommentsFrom(pos_);
    if (idx >= tokens_.size()) return eofToken;
    return tokens_[idx];
}

Token TokenStream::advance() {
    size_t idx = skipCommentsFrom(pos_);
    if (idx >= tokens_.size()) {
        ++pos_;
        return eofToken;
    }
    const Token& tok = tokens_[idx];
    pos_ = idx + 1;
    return tok;
}

bool TokenStream::check(TokenType type) const {
    return !isAtEnd() && peek().type == type;
}

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
    SourceLocation loc = currentLoc();
    diagnostic::error(DiagnosticCategory::Syntax, filePath_, loc, code, {msg});
    // Return a dummy token for recovery
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

TokenType TokenStream::peekNextType() const {
    return peekNext().type;
}

const Token& TokenStream::peekNext() const {
    size_t idx = skipCommentsFrom(pos_);
    if (idx >= tokens_.size()) return eofToken;
    // Move one step forward (skip the current non‑comment token)
    size_t nextIdx = skipCommentsFrom(idx + 1);
    if (nextIdx >= tokens_.size()) return eofToken;
    return tokens_[nextIdx];
}

const Token& TokenStream::peekAt(size_t offset) const {
    // Raw access – caller must handle comments manually if needed.
    // This is used only in lookahead that already uses skipCommentsFrom.
    if (offset >= tokens_.size()) return eofToken;
    return tokens_[offset];
}

// ============================================================================
// Parser construction
// ============================================================================

Parser::Parser(std::vector<Token> tokens, InternedString filePath,
               StringPool& pool, ASTArena& arena)
    : ts_(std::move(tokens), filePath),
      filePath_(std::move(filePath)),
      pool_(pool), arena_(arena) {}

// ============================================================================
// Error handling
// ============================================================================

void Parser::error(const SourceLocation& loc, DiagCode code, const std::string& msg) {
    diagnostic::error(DiagnosticCategory::Syntax, filePath_, loc, code, {msg});
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

/**
 * @brief Parses any declaration (top-level or local).
 * 
 * Grammar (simplified):
 *   declaration := [ '@' attribute ]* [ 'pub' | 'export' ]? actual_decl
 * 
 *   actual_decl := use_decl | struct_decl | enum_decl | trait_decl
 *                | impl_decl | from_decl | type_decl | var_decl | func_decl
 * 
 * ─── Context Rules ─────────────────────────────────────────────────────────
 *   TopLevel: All declaration types allowed, visibility modifiers allowed
 *   Local:    Only struct, enum, trait, impl, from, type, let, const allowed
 *             Visibility modifiers (pub/export) are FORBIDDEN (E2014)
 *             'use' declarations are FORBIDDEN (E2006)
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Invalid use in local context: reports error, skips entire use declaration
 * - Visibility in local context: reports error, continues
 * - Unknown declaration: reports error, returns nullptr (caller synchronizes)
 * 
 * @param ctx Whether this declaration appears at top level or inside a block
 * @return DeclPtr – parsed declaration node, or nullptr on error
 */
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
        // REJECT 'use' inside local contexts
        if (ctx == DeclContext::Local) {
            errorAt(DiagCode::E2006, "'use' declaration is not allowed inside a block; use declarations must be at top level");
            // Skip the 'use' declaration to recover
            ts_.advance(); // consume 'use'
            
            // Skip the rest of the use declaration to avoid cascading errors
            while (!ts_.isAtEnd() && 
                   !ts_.checkAny({TokenType::SEMICOLON, TokenType::RBRACE, 
                                  TokenType::LET, TokenType::CONST, TokenType::IF,
                                  TokenType::FOR, TokenType::WHILE, TokenType::RETURN})) {
                ts_.advance();
            }
            return nullptr;
        }
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

// ============================================================================
// Precedence Helpers
// ============================================================================
// 
// These functions map token types to precedence levels and operator enums.
// 
//   infixPrec()       : returns precedence for an infix operator
//   tokenToBinaryOp() : converts TokenType → BinaryOp (arithmetic, comparison, etc.)
//   tokenToAssignOp() : converts TokenType → AssignOp (assignment operators)
//   isAssignOp()      : true for assignment operators (lowest precedence)
// 
// The precedence values are used by parsePrattExpr to decide whether to
// consume an operator or stop.
// ============================================================================

int Parser::infixPrec(TokenType t) const {
    switch (t) {
        case TokenType::ASSIGN:
        case TokenType::PLUS_ASSIGN:
        case TokenType::MINUS_ASSIGN:
        case TokenType::MUL_ASSIGN:
        case TokenType::DIV_ASSIGN:
        case TokenType::POW_ASSIGN:
        case TokenType::MOD_ASSIGN:
        case TokenType::BIT_AND_ASSIGN:
        case TokenType::BIT_OR_ASSIGN:
        case TokenType::BIT_XOR_ASSIGN:
        case TokenType::SHL_ASSIGN:
        case TokenType::SHR_ASSIGN:
            return PREC_ASSIGN;
        case TokenType::COMPOSE:            return PREC_COMPOSE;
        case TokenType::PIPELINE:           return PREC_PIPE;
        case TokenType::QUESTION_QUESTION:  return PREC_NULLCOAL;
        case TokenType::OR:                 return PREC_OR;
        case TokenType::AND:                return PREC_AND;
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_EQUAL_EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::LESS:
        case TokenType::GREATER:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER_EQUAL:
        case TokenType::IS:
            return PREC_CMP;
        case TokenType::BIT_AND:
        case TokenType::BIT_OR:
        case TokenType::BIT_XOR:
        case TokenType::SHL:
        case TokenType::SHR:
            return PREC_BITWISE;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return PREC_ADD;
        case TokenType::MUL:
        case TokenType::DIV:
        case TokenType::MOD:
            return PREC_MUL;
        case TokenType::POW:
            return PREC_POW;
        default:
            return PREC_NONE;
    }
}

BinaryOp Parser::tokenToBinaryOp(TokenType t) const {
    switch (t) {
        case TokenType::PLUS:                return BinaryOp::Add;
        case TokenType::MINUS:               return BinaryOp::Sub;
        case TokenType::MUL:                 return BinaryOp::Mul;
        case TokenType::DIV:                 return BinaryOp::Div;
        case TokenType::POW:                 return BinaryOp::Pow;
        case TokenType::MOD:                 return BinaryOp::Mod;
        case TokenType::EQUAL_EQUAL:         return BinaryOp::Eq;
        case TokenType::EQUAL_EQUAL_EQUAL:   return BinaryOp::RefEq;
        case TokenType::NOT_EQUAL:           return BinaryOp::Ne;
        case TokenType::LESS:                return BinaryOp::Lt;
        case TokenType::GREATER:             return BinaryOp::Gt;
        case TokenType::LESS_EQUAL:          return BinaryOp::Le;
        case TokenType::GREATER_EQUAL:       return BinaryOp::Ge;
        case TokenType::AND:                 return BinaryOp::And;
        case TokenType::OR:                  return BinaryOp::Or;
        case TokenType::BIT_AND:             return BinaryOp::BitAnd;
        case TokenType::BIT_OR:              return BinaryOp::BitOr;
        case TokenType::BIT_XOR:             return BinaryOp::BitXor;
        case TokenType::SHL:                 return BinaryOp::Shl;
        case TokenType::SHR:                 return BinaryOp::Shr;
        default:                             return BinaryOp::Add;
    }
}

AssignOp Parser::tokenToAssignOp(TokenType t) const {
    switch (t) {
        case TokenType::ASSIGN:          return AssignOp::Assign;
        case TokenType::PLUS_ASSIGN:     return AssignOp::AddAssign;
        case TokenType::MINUS_ASSIGN:    return AssignOp::SubAssign;
        case TokenType::MUL_ASSIGN:      return AssignOp::MulAssign;
        case TokenType::DIV_ASSIGN:      return AssignOp::DivAssign;
        case TokenType::POW_ASSIGN:      return AssignOp::PowAssign;
        case TokenType::MOD_ASSIGN:      return AssignOp::ModAssign;
        case TokenType::BIT_AND_ASSIGN:  return AssignOp::BitAndAssign;
        case TokenType::BIT_OR_ASSIGN:   return AssignOp::BitOrAssign;
        case TokenType::BIT_XOR_ASSIGN:  return AssignOp::BitXorAssign;
        case TokenType::SHL_ASSIGN:      return AssignOp::ShlAssign;
        case TokenType::SHR_ASSIGN:      return AssignOp::ShrAssign;
        default:                         return AssignOp::Assign;
    }
}

bool Parser::isAssignOp(TokenType t) const {
    switch (t) {
        case TokenType::ASSIGN:
        case TokenType::PLUS_ASSIGN:
        case TokenType::MINUS_ASSIGN:
        case TokenType::MUL_ASSIGN:
        case TokenType::DIV_ASSIGN:
        case TokenType::POW_ASSIGN:
        case TokenType::MOD_ASSIGN:
        case TokenType::BIT_AND_ASSIGN:
        case TokenType::BIT_OR_ASSIGN:
        case TokenType::BIT_XOR_ASSIGN:
        case TokenType::SHL_ASSIGN:
        case TokenType::SHR_ASSIGN:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Pratt Parser Core
// ============================================================================
// 
// The Pratt parser is a recursive‑descent operator precedence parser.
// 
//   parseExpr()          : entry point, starts at lowest precedence
//   parsePrattExpr()     : main climbing algorithm
//   parsePrefixExpr()    : handles unary operators and primary expressions
//   parsePostfixExpr()   : handles calls, indexing, field access after the prefix
// 
// Algorithm (in parsePrattExpr):
//   1. Parse a prefix expression (unary op or primary)
//   2. Apply all postfix operators (calls, indexing, ., :, ?.)
//   3. While next token is infix operator with precedence > minPrec:
//        a. If assignment operator → parseInfixAssign, break
//        b. If 'is' → parseInfixIs, continue
//        c. If '|>' → parsePipelineExpr, continue
//        d. If '+>' → parseComposeExpr, continue
//        e. If '??' → parseInfixNullCoalesce, break
//        f. Otherwise → parseInfixBinary, then re‑apply postfix
// 
// Right‑associative operators (^, ??, assignment) use `prec - 1` when recursing.
// ============================================================================

ExprPtr Parser::parseExpr(bool allowStructLiteral) {
    return parsePrattExpr(PREC_NONE, allowStructLiteral);
}

ExprPtr Parser::parsePrattExpr(int minPrec, bool allowStructLiteral) {
    ExprPtr lhs = parsePrefixExpr(allowStructLiteral);
    if (!lhs) {
        return arena_.make<UnknownExprAST>();
    }

    lhs = parsePostfixExpr(std::move(lhs));

    while (true) {
        int prec = infixPrec(ts_.peekType());
        if (prec <= minPrec) break;

        TokenType opTok = ts_.peekType();

        if (isAssignOp(opTok)) {
            lhs = parseInfixAssign(std::move(lhs), allowStructLiteral);
            break;
        }

        if (opTok == TokenType::IS) {
            lhs = parseInfixIs(std::move(lhs));
            continue;
        }

        if (opTok == TokenType::PIPELINE) {
            lhs = parsePipelineExpr(std::move(lhs));
            continue;
        }

        if (opTok == TokenType::COMPOSE) {
            lhs = parseComposeExpr(std::move(lhs));
            continue;
        }

        if (opTok == TokenType::QUESTION_QUESTION) {
            lhs = parseInfixNullCoalesce(std::move(lhs), allowStructLiteral);
            break;
        }

        lhs = parseInfixBinary(std::move(lhs), opTok, prec, allowStructLiteral);
        lhs = parsePostfixExpr(std::move(lhs));
    }

    return lhs;
}

// ============================================================================
// Prefix & Primary Parsers
// ============================================================================
// 
//   parsePrefixExpr() : handles unary operators (-, not, ~~, &) then calls
//                       parsePrimaryExpr() for the operand.
//   parsePrimaryExpr(): dispatches to expression atom parsers based on the
//                       current token. Dispatch order is critical:
// 
// Dispatch Priority (highest to lowest):
//   1. match expression    : 'match'
//   2. if expression       : 'if' (expression form, requires '??' and 'else')
//   3. #intrinsic call     : '#'
//   4. await               : 'await'
//   5. array literal       : '['
//   6. bare '{' error      : suggests missing struct name or match
//   7. anonymous function  : looksLikeAnonFunc()
//   8. grouped expression  : '(' expr ')'
//   9. '*' unsafe cast     : '*T(expr)'
//   10. identifier         : IDENTIFIER (struct literal, behavior, or plain)
//   11. primitive type cast: 'float(x)'
//   12. literal            : numbers, strings, true, false, nil
// 
// The allowStructLiteral flag controls whether IDENTIFIER '{' is parsed as a
// struct literal. It is disabled in contexts where '{' belongs to something
// else (e.g., after 'if' or 'match').
// ============================================================================

ExprPtr Parser::parsePrefixExpr(bool allowStructLiteral) {
    SourceLocation loc = ts_.currentLoc();

    switch (ts_.peekType()) {
        case TokenType::MINUS: {
            ts_.advance();
            ExprPtr operand = parsePrefixExpr(allowStructLiteral);
            if (!operand) {
                errorAt(DiagCode::E2008, "expected expression after '-'");
                return arena_.make<UnknownExprAST>();
            }
            auto node = arena_.make<UnaryExprAST>();
            node->loc = loc;
            node->op = UnaryOp::Neg;
            node->operand = std::move(operand);
            return node;
        }
        case TokenType::NOT: {
            ts_.advance();
            ExprPtr operand = parsePrefixExpr(allowStructLiteral);
            if (!operand) {
                errorAt(DiagCode::E2008, "expected expression after 'not'");
                return arena_.make<UnknownExprAST>();
            }
            auto node = arena_.make<UnaryExprAST>();
            node->loc = loc;
            node->op = UnaryOp::Not;
            node->operand = std::move(operand);
            return node;
        }
        case TokenType::BIT_NOT: {
            ts_.advance();
            ExprPtr operand = parsePrefixExpr(allowStructLiteral);
            if (!operand) {
                errorAt(DiagCode::E2008, "expected expression after '~'");
                return arena_.make<UnknownExprAST>();
            }
            auto node = arena_.make<UnaryExprAST>();
            node->loc = loc;
            node->op = UnaryOp::BitNot;
            node->operand = std::move(operand);
            return node;
        }
        case TokenType::AMPERSAND: {
            ts_.advance();
            ExprPtr operand = parsePrefixExpr(allowStructLiteral);
            if (!operand) {
                errorAt(DiagCode::E2008, "expected expression after '&'");
                return arena_.make<UnknownExprAST>();
            }
            auto node = arena_.make<UnaryExprAST>();
            node->loc = loc;
            node->op = UnaryOp::Ref;
            node->operand = std::move(operand);
            return node;
        }
        default:
            return parsePrimaryExpr(allowStructLiteral);
    }
}

/**
 * @brief Parses a primary expression (atom) from the token stream.
 * 
 * Dispatch Priority (highest to lowest):
 *   1. `match` expression
 *   2. `if` expression (expression form)
 *   3. `resolve` expression          // NEW
 *   4. `#intrinsic` call
 *   5. `await` expression
 *   6. array literal `[...]`
 *   7. bare `{` (error)
 *   8. anonymous function
 *   9. grouped expression `(expr)`
 *   10. unsafe cast `*T(expr)`
 *   11. identifier (struct literal, behavior access, or plain)
 *   12. primitive type cast `T(expr)`
 *   13. literal (numbers, strings, true, false, nil)
 * 
 * @param allowStructLiteral When false, `IDENTIFIER '{'` is NOT parsed as a
 *        struct literal (used in contexts like `if` conditions).
 * 
 * @return ExprPtr – the parsed expression, or UnknownExprAST on error
 */
ExprPtr Parser::parsePrimaryExpr(bool allowStructLiteral) {
    SourceLocation loc = ts_.currentLoc();

    // match expression
    if (ts_.check(TokenType::MATCH)) {
        return parseMatchExpr();
    }

    // if expression
    if (ts_.check(TokenType::IF)) {
        return parseIfExpr();
    }

    // resolve expression
    if (ts_.check(TokenType::RESOLVE)) {
        return parseResolveExpr();
    }

    // #intrinsic call
    if (ts_.check(TokenType::HASH)) {
        return parseIntrinsicCallExpr();
    }

    // await
    if (ts_.check(TokenType::AWAIT)) {
        return parseAwaitExpr();
    }

    // array literal
    if (ts_.check(TokenType::LBRACKET)) {
        return parseArrayLiteralExpr();
    }

    // bare '{' error
    if (ts_.check(TokenType::LBRACE)) {
        errorAt(DiagCode::E2007, "unexpected block in expression position");
        int braceDepth = 1;
        ts_.advance();
        while (!ts_.isAtEnd() && braceDepth > 0) {
            if (ts_.match(TokenType::LBRACE)) braceDepth++;
            else if (ts_.match(TokenType::RBRACE)) braceDepth--;
            else ts_.advance();
        }
        return arena_.make<UnknownExprAST>();
    }

    // anonymous function
    if (looksLikeAnonFunc()) {
        return parseAnonFuncExpr();
    }

    // grouped expression
    if (ts_.check(TokenType::LPAREN)) {
        ts_.advance();
        ExprPtr inner = parsePrattExpr(PREC_NONE, allowStructLiteral);
        if (!ts_.check(TokenType::RPAREN)) {
            errorAt(DiagCode::E2001, "expected ')' to close grouped expression");
        } else {
            ts_.advance();
        }
        return inner;
    }

    // '*' unsafe cast
    if (ts_.check(TokenType::MUL) && looksLikeType()) {
        ts_.advance();
        TypePtr targetType = parseBaseType();
        if (!targetType) {
            errorAt(DiagCode::E2005, "expected type after '*' in unsafe cast");
            return arena_.make<UnknownExprAST>();
        }
        if (!ts_.check(TokenType::LPAREN)) {
            errorAt(DiagCode::E2001, "expected '(' after type in unsafe cast");
            return arena_.make<UnknownExprAST>();
        }
        return parseTypeConvExpr(true, std::move(targetType));
    }

    // identifier
    if (ts_.check(TokenType::IDENTIFIER)) {
        std::string name = ts_.peek().value;

        // struct literal
        if (allowStructLiteral && looksLikeStructLiteral()) {
            ts_.advance();
            ArenaSpan<TypePtr> genericArgs;
            if (ts_.check(TokenType::LESS)) {
                genericArgs = parseGenericArgs();
            }
            return parseStructLiteralExpr(name, genericArgs);
        }

        // behavior access
        if (looksLikeBehaviorAccess()) {
            std::string typeName = ts_.advance().value;
            ArenaSpan<TypePtr> genericArgs;
            if (ts_.check(TokenType::LESS)) {
                genericArgs = parseGenericArgs();
            }
            ts_.consume(TokenType::COLON, "expected ':' in behavior access");
            if (!ts_.check(TokenType::IDENTIFIER)) {
                errorAt(DiagCode::E2003, "expected method name after ':'");
                return arena_.make<UnknownExprAST>();
            }
            std::string method = ts_.advance().value;

            auto node = arena_.make<BehaviorAccessExprAST>();
            node->loc = loc;
            node->typeName = pool_.intern(typeName);
            node->genericArgs = genericArgs;
            node->method = pool_.intern(method);
            node->isBehaviorMember = true;
            return node;
        }

        // plain identifier
        ts_.advance();
        auto node = arena_.make<IdentifierExprAST>(pool_.intern(name));
        node->loc = loc;
        return node;
    }

    // primitive type cast
    if (looksLikeType() && ts_.peekNextType() == TokenType::LPAREN) {
        TypePtr targetType = parsePrimitiveType();
        if (targetType && ts_.check(TokenType::LPAREN)) {
            return parseTypeConvExpr(false, std::move(targetType));
        }
    }

    // literal
    return parseLiteralExpr();
}

// ============================================================================
// Postfix Parser
// ============================================================================
// 
// parsePostfixExpr applies postfix operators to an already‑parsed left‑hand
// side expression. It handles:
// 
//   - Function call      : lhs(args)
//   - Generic call       : lhs<T>(args)
//   - Indexing           : lhs[expr] or lhs[start..end]
//   - Field access       : lhs.field
//   - Nullable chain     : lhs?.field (collects multiple steps)
// 
// Important: This does NOT handle '|>', '+>', or ':' (method access) – those
// are handled at higher precedence levels in parsePrattExpr.
// 
// Nullable chains are collected in one go: each '?.' appends a field name to
// a single NullableChainExprAST. The grammar requires every '?.' chain to be
// terminated by '??' – this is enforced by parseInfixNullCoalesce.
// ============================================================================

ExprPtr Parser::parsePostfixExpr(ExprPtr lhs) {
    while (true) {
        if (ts_.check(TokenType::RPAREN)) break;
        if (ts_.check(TokenType::PIPELINE) || ts_.check(TokenType::COMPOSE)) break;

        // function call
        if (ts_.check(TokenType::LPAREN)) {
            lhs = parseCallExpr(std::move(lhs), ArenaSpan<TypePtr>());
            continue;
        }

        // generic call
        if (ts_.check(TokenType::LESS) && 
            (lhs->isa<IdentifierExprAST>() || lhs->isa<BehaviorAccessExprAST>())) {
            
            size_t savedPos = ts_.getPos();
            int depth = 1;
            size_t i = ts_.getPos() + 1;
            const auto& tokens = ts_.getTokens();
            size_t tokenCount = ts_.getTokenCount();
            
            while (i < tokenCount && depth > 0) {
                if (tokens[i].type == TokenType::LESS) ++depth;
                else if (tokens[i].type == TokenType::GREATER) --depth;
                else if (tokens[i].type == TokenType::EOF_TOKEN) break;
                ++i;
            }
            
            if (depth == 0 && i + 1 < tokenCount && tokens[i + 1].type == TokenType::LPAREN) {
                ArenaSpan<TypePtr> genericArgs = parseGenericArgs();
                lhs = parseCallExpr(std::move(lhs), genericArgs);
                continue;
            }
        }

        // index/slice
        if (ts_.check(TokenType::LBRACKET)) {
            lhs = parseIndexExpr(std::move(lhs));
            continue;
        }

        // field access
        if (ts_.check(TokenType::DOT)) {
            ts_.advance();
            if (!ts_.check(TokenType::IDENTIFIER)) {
                errorAt(DiagCode::E2003, "expected field name after '.'");
                break;
            }
            std::string field = ts_.advance().value;
            auto node = arena_.make<FieldAccessExprAST>();
            node->loc = lhs->loc;
            node->object = std::move(lhs);
            node->field = pool_.intern(field);
            lhs = std::move(node);
            continue;
        }

        // nullable chain
        if (ts_.check(TokenType::QUESTION_DOT)) {
            // Collect all consecutive '?.' steps in one go
            std::vector<InternedString> steps;
            ExprPtr object = std::move(lhs);
            
            while (ts_.check(TokenType::QUESTION_DOT)) {
                ts_.advance(); // consume '?.'
                if (!ts_.check(TokenType::IDENTIFIER)) {
                    errorAt(DiagCode::E2003, "expected field name after '?.'");
                    break;
                }
                steps.push_back(pool_.intern(ts_.advance().value));
            }
            
            auto chain = arena_.make<NullableChainExprAST>();
            chain->loc = object->loc;
            chain->object = std::move(object);
            
            auto builder = arena_.makeBuilder<InternedString>();
            for (auto& s : steps) builder.push_back(std::move(s));
            chain->steps = builder.build();
            
            lhs = std::move(chain);
            continue;
        }

        break;
    }

    return lhs;
}

// ============================================================================
// Lvalue Parsing (for Assignments)
// ============================================================================
// 
// parseLvalue() parses an assignable left‑hand side expression for multi‑assignment.
// 
// Grammar: IDENTIFIER { ( '.' IDENTIFIER ) | ( '[' expr ']' ) }
// 
// Examples: x, point.x, arr[i], matrix[row][col]
// 
// This is distinct from parseExpr() because it stops before operators like '='
// and does not allow behavior access (':') or function calls.
// ============================================================================

ExprPtr Parser::parseLvalue() {
    if (!ts_.check(TokenType::IDENTIFIER)) {
        errorAt(DiagCode::E2003, "expected identifier for lvalue");
        return nullptr;
    }
    std::string name = ts_.advance().value;
    ExprPtr expr = arena_.make<IdentifierExprAST>(pool_.intern(name));
    expr->loc = ts_.currentLoc();

    while (true) {
        if (ts_.check(TokenType::DOT)) {
            ts_.advance();
            if (!ts_.check(TokenType::IDENTIFIER)) {
                errorAt(DiagCode::E2003, "expected field name after '.'");
                return expr;
            }
            std::string field = ts_.advance().value;
            auto node = arena_.make<FieldAccessExprAST>();
            node->loc = expr->loc;
            node->object = std::move(expr);
            node->field = pool_.intern(field);
            expr = std::move(node);
        } 
        else if (ts_.check(TokenType::LBRACKET)) {
            ts_.advance();
            ExprPtr index = parseExpr();
            if (!index) {
                errorAt(DiagCode::E2008, "expected index expression");
                return expr;
            }
            ts_.consume(TokenType::RBRACKET, "expected ']' after index");
            auto node = arena_.make<IndexExprAST>();
            node->loc = expr->loc;
            node->target = std::move(expr);
            node->index = std::move(index);
            node->kind = IndexKind::Element;
            expr = std::move(node);
        }
        else if (ts_.check(TokenType::COLON)) {
            break;
        }
        else {
            break;
        }
    }
    return expr;
}