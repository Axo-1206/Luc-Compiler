/**
 * @file PipelineParser.cpp
 * @brief Parses the pipeline operator (`|>`) for runtime function chaining.
 * 
 * ============================================================================
 * FILE OVERVIEW
 * ============================================================================
 * 
 * This file implements parsing of pipeline expressions where the output of one
 * function becomes the input to the next, executing left‑to‑right at runtime.
 * 
 * Grammar (from LUC_GRAMMAR.md):
 *   pipeline_expr   := pipeline_seed { '|>' pipeline_step }
 *   pipeline_seed   := expr
 *   pipeline_step   := func_ref [ '(' arg_list ')' '!' ] | anon_func
 * 
 *   func_ref := IDENTIFIER
 *             | IDENTIFIER '.' IDENTIFIER
 *             | IDENTIFIER ':' IDENTIFIER
 *             | func_ref generic_args
 * 
 * Examples:
 *   42 |> float |> sqrt
 *   getUser(id) |> validate |> save
 *   v |> Vec2:normalize |> scale(2.0)!
 *   numbers |> filter<int>(isPositive)! |> sum
 * 
 * ─── Important Rules ───────────────────────────────────────────────────────
 *   - The pipeline short‑circuits on Error when the error library is used.
 *   - Steps with `~async` are allowed; the entire pipeline becomes async and
 *     must be awaited.
 *   - Steps with `~nullable` are forbidden – guard before the pipeline.
 *   - Steps with `~parallel` are forbidden – pipeline execution is synchronous.
 *   - Curry functions cannot be used directly as steps; pre‑apply all but the
 *     last group first (e.g., `let addTen = add(10); 42 |> addTen`).
 *   - The `!` argument pack annotation marks an intentionally incomplete
 *     argument list; the upstream value is injected as the first argument.
 * 
 * ─── Error Recovery Strategy ──────────────────────────────────────────────
 *   parsePipelineStep() NEVER returns nullptr. On failure, it returns a step
 *   whose `callable` is an `UnknownExprAST`. It also consumes tokens until
 *   the next `|>` or a safe boundary (semicolon, brace, EOF). This allows
 *   the pipeline loop to continue parsing subsequent steps after an error.
 *   The resulting AST contains error placeholders, and semantic analysis
 *   can skip or report them.
 * 
 * @see ParserExpr.cpp for Pratt parser integration
 * @see ParserHelpers.cpp for parseFuncRef
 */

#include "parser/Parser.hpp"
#include "ast/support/InternedString.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugUtils.hpp"
#include "debug/DebugMacros.hpp"

// ============================================================================
// Pipeline Expression
// ============================================================================

ExprPtr Parser::parsePipelineExpr(ExprPtr seed) {
    LUC_LOG_EXPR_VERBOSE("parsePipelineExpr: entering");
    
    if (!seed) {
        LUC_LOG_EXPR("parsePipelineExpr: ERROR - expected pipeline seed before '|>'");
        errorAt(DiagCode::E1008, "expected pipeline seed before '|>'");
        return arena_.make<UnknownExprAST>();
    }

    std::vector<PipelineStepPtr> steps;
    int stepCount = 0;

    while (ts_.check(TokenType::PIPELINE)) {
        LUC_LOG_EXPR_EXTREME("parsePipelineExpr: found '|>' operator #" << stepCount + 1);
        ts_.advance();  // consume '|>'
        steps.push_back(parsePipelineStep());
        stepCount++;
    }

    if (steps.empty()) {
        // No '|>' operators were found – this is not a pipeline
        LUC_LOG_EXPR_EXTREME("parsePipelineExpr: no pipeline steps, returning seed");
        return seed;
    }

    auto node = arena_.make<PipelineExprAST>();
    node->loc = seed->loc;
    node->seed = std::move(seed);

    auto builder = arena_.makeBuilder<PipelineStepPtr>();
    for (auto& s : steps) builder.push_back(std::move(s));
    node->steps = builder.build();

    LUC_LOG_EXPR_VERBOSE("parsePipelineExpr: parsed " << stepCount << " step(s)");
    return node;
}

// ============================================================================
// Pipeline Step
// ============================================================================

PipelineStepPtr Parser::parsePipelineStep() {
    LUC_LOG_EXPR_EXTREME("parsePipelineStep: entering");
    
    // 1. Anonymous function step
    if (looksLikeAnonFunc()) {
        LUC_LOG_EXPR_EXTREME("parsePipelineStep: anonymous function step");
        auto step = arena_.make<PipelineStepAST>();
        step->loc = ts_.currentLoc();
        step->callable = parseAnonFuncExpr();
        return step;
    }

    // 2. Parse function reference (may be generic, dotted, method)
    ExprPtr callable = parseFuncRef();

    // 3. Handle parse failure
    if (!callable || callable->isa<UnknownExprAST>()) {
        LUC_LOG_EXPR("parsePipelineStep: ERROR - expected function reference or anonymous function");
        errorAt(DiagCode::E1002,
                "expected function name, method reference, or anonymous function");

        // Create error placeholder step
        auto step = arena_.make<PipelineStepAST>();
        step->loc = ts_.currentLoc();
        step->callable = arena_.make<UnknownExprAST>();

        // Recover: skip to next pipeline operator or safe boundary
        while (!ts_.isAtEnd() &&
               !ts_.check(TokenType::PIPELINE) &&
               !ts_.check(TokenType::SEMICOLON) &&
               !ts_.check(TokenType::RBRACE) &&
               !ts_.check(TokenType::EOF_TOKEN)) {
            ts_.advance();
        }

        return step;
    }

    // 4. Success – create step with parsed callable
    auto step = arena_.make<PipelineStepAST>();
    step->loc = callable->loc;
    step->callable = std::move(callable);
    
    LUC_LOG_EXPR_EXTREME("parsePipelineStep: callable parsed");

    // 5. Optional argument pack: '(' arg_list ')' '!'
    if (ts_.check(TokenType::LPAREN)) {
        LUC_LOG_EXPR_EXTREME("parsePipelineStep: parsing argument pack");
        ts_.advance();

        std::vector<ExprPtr> packArgs;
        int consecutiveErrors = 0;
        const int MAX_CONSECUTIVE_ERRORS = 5;
        int argCount = 0;

        // Parse comma‑separated expressions until closing ')'
        while (!ts_.check(TokenType::RPAREN) && !ts_.isAtEnd()) {
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                LUC_LOG_EXPR("parsePipelineStep: ERROR - too many consecutive errors in argument pack");
                errorAt(DiagCode::E1002, "too many consecutive errors in argument pack; skipping to ')'");
                while (!ts_.isAtEnd() && !ts_.check(TokenType::RPAREN)) ts_.advance();
                break;
            }

            size_t savedPos = ts_.getPos();
            ExprPtr arg = parseExpr();

            if (ts_.getPos() == savedPos) {
                LUC_LOG_EXPR("parsePipelineStep: ERROR - expected argument expression");
                errorAt(DiagCode::E1008, "expected argument expression");
                if (!ts_.isAtEnd()) ts_.advance();
                consecutiveErrors++;
                if (ts_.check(TokenType::COMMA)) ts_.advance();
                continue;
            }

            consecutiveErrors = 0;
            argCount++;
            LUC_LOG_EXPR_EXTREME("parsePipelineStep: argument #" << argCount);
            packArgs.push_back(std::move(arg));

            if (ts_.check(TokenType::RPAREN)) break;
            if (!ts_.match(TokenType::COMMA)) {
                LUC_LOG_EXPR("parsePipelineStep: ERROR - expected ',' after argument");
                errorAt(DiagCode::E1001, "expected ',' after argument");
                while (!ts_.isAtEnd() && !ts_.check(TokenType::COMMA) && !ts_.check(TokenType::RPAREN)) {
                    ts_.advance();
                }
                if (ts_.check(TokenType::COMMA)) ts_.advance();
                break;
            }
        }

        ts_.consume(TokenType::RPAREN, "expected ')'");

        if (!ts_.match(TokenType::BANG)) {
            LUC_LOG_EXPR("parsePipelineStep: ERROR - expected '!' after arguments");
            errorAt(DiagCode::E1001,
                    "expected '!' after arguments for argument pack");
            // Still return the step – it just won't have packArgs
            return step;
        }

        auto builder = arena_.makeBuilder<ExprPtr>();
        for (auto& a : packArgs) builder.push_back(std::move(a));
        step->packArgs = builder.build();
        
        LUC_LOG_EXPR_EXTREME("parsePipelineStep: argument pack with " << argCount << " argument(s)");
    }

    return step;
}