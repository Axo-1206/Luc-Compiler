#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

/**
 * @brief Parses a function declaration.
 * 
 * Grammar:
 *   `let`/`const` IDENTIFIER [ `<` generic_params `>` ]
 *   [ `~async` | `~nullable` | `~parallel` ]*
 *   param_group+ [ `->` return_list ] [ `=` body ]
 * 
 * Example: `let add ~async (a int)(b int) -> int = { return a + b }`
 * 
 * ─── Token Consumption ─────────────────────────────────────────────────────
 * On entry: positioned at function name (keyword already consumed)
 * On exit:  positioned after the body (or after the signature if no body)
 * 
 * ─── Body Parsing Variants ─────────────────────────────────────────────────
 *   1. Block body      : `= { ... }`
 *   2. Verbose anon-func: `= (params) -> ret { ... }`
 *   3. Expression body  : `= expr` (wrapped in ReturnStmt + BlockStmt)
 *   4. No body          : (valid for `@extern` declarations)
 * 
 * ─── Parameter Groups ──────────────────────────────────────────────────────
 *   - Function may have multiple `(params)` groups (currying)
 *   - Parameters are flattened into `allParams` with `groupSizes` tracking
 * 
 * ─── Error Recovery ───────────────────────────────────────────────────────
 * - Missing '(' after name: reports error, returns nullptr
 * - Missing body after '=': reports error, returns nullptr
 * - Invalid parameter group: skip group, continue parsing
 */
ASTPtr<FuncDeclAST> Parser::parseFuncDecl(DeclKeyword kw, Visibility vis) {
    LUC_LOG_DECL_VERBOSE("parseFuncDecl: entering, kw=" << (kw == DeclKeyword::Let ? "let" : "const"));
    SourceLocation loc = ts_.currentLoc();

    if (!ts_.check(TokenType::IDENTIFIER)) {
        LUC_LOG_DECL("parseFuncDecl: ERROR - expected function name");
        errorAt(DiagCode::E1003, "expected function name");
        return nullptr;
    }
    InternedString name = pool_.intern(ts_.advance().value);
    LUC_LOG_DECL_EXTREME("parseFuncDecl: function name = " << pool_.lookup(name));
    
    auto node = arena_.make<FuncDeclAST>();
    node->loc = loc;
    node->keyword = kw;
    node->name = name;
    node->visibility = vis;

    if (ts_.check(TokenType::LESS)) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: parsing generic parameters");
        node->genericParams = parseGenericParams();
        LUC_LOG_DECL_EXTREME("parseFuncDecl: " << node->genericParams.size() << " generic parameter(s)");
    }

    // Create a FuncTypeAST to hold signature and qualifiers
    auto funcType = arena_.make<FuncTypeAST>();
    funcType->loc = loc;

    // Parse raw qualifiers and build bitmask
    std::vector<InternedString> rawQuals;
    uint32_t qualMask = 0;
    int qualifierCount = 0;
    
    while (ts_.check(TokenType::TILDE)) {
        ts_.advance();
        if (!ts_.check(TokenType::IDENTIFIER)) {
            LUC_LOG_DECL("parseFuncDecl: ERROR - expected qualifier name after '~'");
            errorAt(DiagCode::E1003, "expected qualifier name after '~'");
            break;
        }
        InternedString q = pool_.intern(ts_.advance().value);
        rawQuals.push_back(q);
        qualifierCount++;
        std::string_view qstr = pool_.lookup(q);
        LUC_LOG_DECL_EXTREME("parseFuncDecl: qualifier ~" << qstr);
        
        // Set bit based on qualifier name (semantic pass will validate)
        if (qstr == "async") qualMask |= QualifierBits::Async;
        else if (qstr == "nullable") qualMask |= QualifierBits::Nullable;
        else if (qstr == "parallel") qualMask |= QualifierBits::Parallel;
        // Other qualifiers are ignored here; semantic pass reports errors
    }
    
    if (qualifierCount > 0) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: " << qualifierCount << " qualifier(s)");
    }
    
    auto qBuilder = arena_.makeBuilder<InternedString>();
    for (auto& q : rawQuals) qBuilder.push_back(std::move(q));
    funcType->rawQualifiers = qBuilder.build();
    funcType->qualifiers = qualMask;

    // Parameter groups: accumulate flat
    std::vector<ParamPtr> allParams;
    std::vector<size_t> groupSizes;
    int groupCount = 0;
    
    if (!ts_.check(TokenType::LPAREN)) {
        LUC_LOG_DECL("parseFuncDecl: ERROR - expected '(' to start parameter list");
        errorAt(DiagCode::E1001, "expected '(' to start parameter list for function '" + std::string(pool_.lookup(name)) + "'");
        return nullptr;
    }
    
    while (ts_.check(TokenType::LPAREN)) {
        groupCount++;
        std::vector<ParamPtr> group = parseParamGroup();
        groupSizes.push_back(group.size());
        LUC_LOG_DECL_EXTREME("parseFuncDecl: parameter group #" << groupCount 
                             << " has " << group.size() << " parameter(s)");
        for (auto& p : group) {
            allParams.push_back(std::move(p));
        }
    }
    
    auto paramsBuilder = arena_.makeBuilder<ParamPtr>();
    for (auto& p : allParams) paramsBuilder.push_back(std::move(p));
    funcType->sig.allParams = paramsBuilder.build();

    auto gsBuilder = arena_.makeBuilder<size_t>();
    for (auto& sz : groupSizes) gsBuilder.push_back(sz);
    funcType->sig.groupSizes = gsBuilder.build();
    
    LUC_LOG_DECL_EXTREME("parseFuncDecl: total " << allParams.size() << " parameters");

    // Return types
    if (ts_.match(TokenType::ARROW)) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: parsing return types");
        funcType->sig.returnTypes = parseReturnList();
        LUC_LOG_DECL_EXTREME("parseFuncDecl: " << funcType->sig.returnTypes.size() << " return type(s)");
    }

    // Store the complete function type in the declaration
    node->funcType = std::move(funcType);

    // Body (unchanged)
    if (!ts_.check(TokenType::ASSIGN)) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: no body (extern or forward declaration)");
        ts_.match(TokenType::SEMICOLON);
        return node;
    }

    LUC_LOG_DECL_EXTREME("parseFuncDecl: parsing body");
    ts_.advance();

    if (ts_.check(TokenType::LBRACE)) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: block body");
        node->body = parseBlock();
    } else if (ts_.check(TokenType::LPAREN)) {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: verbose anon-func body");
        if (!ts_.check(TokenType::LBRACE)) {
            LUC_LOG_DECL("parseFuncDecl: ERROR - expected '{' to start function body");
            errorAt(DiagCode::E1001, "expected '{' to start function body");
            return nullptr;
        }
        node->body = parseBlock();
    } else {
        LUC_LOG_DECL_EXTREME("parseFuncDecl: expression body (will be wrapped)");
        SourceLocation bodyLoc = ts_.currentLoc();
        ExprPtr expr = parseExpr();
        if (!expr) {
            LUC_LOG_DECL("parseFuncDecl: ERROR - expected expression after '='");
            errorAt(DiagCode::E1008, "expected expression after '='");
            return nullptr;
        }

        auto ret = arena_.make<ReturnStmtAST>();
        ret->loc = bodyLoc;
        std::vector<ExprPtr> vals;
        vals.push_back(std::move(expr));
        auto valsBuilder = arena_.makeBuilder<ExprPtr>();
        for (auto& v : vals) valsBuilder.push_back(std::move(v));
        ret->values = valsBuilder.build();

        auto block = arena_.make<BlockStmtAST>();
        block->loc = bodyLoc;
        std::vector<StmtPtr> stmts;
        stmts.push_back(std::move(ret));
        auto stmtsBuilder = arena_.makeBuilder<StmtPtr>();
        for (auto& s : stmts) stmtsBuilder.push_back(std::move(s));
        block->stmts = stmtsBuilder.build();

        node->body = std::move(block);
    }
    
    ts_.match(TokenType::SEMICOLON);
    LUC_LOG_DECL_VERBOSE("parseFuncDecl: success");
    return node;
}