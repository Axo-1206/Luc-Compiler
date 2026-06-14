/**
 * @file DeclStmtChecker.cpp
 * @brief Implementation of declaration statement checking.
 * 
 * Handles: DeclStmtAST
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"

void checkDeclStmt(DeclStmtAST* declStmt, SemanticContext& ctx, TypeAST* /*expectedReturn*/) {
    LUC_LOG_SEMANTIC_EXTREME("checkDeclStmt");
    
    if (!declStmt->decl) return;
    
    // Local declarations are registered in the current scope by the collector
    // Here we just need to check initializers
    if (auto* var = declStmt->decl->as<VarDeclAST>()) {
        if (var->init) {
            TypeAST* initType = checkExpr(var->init, ctx);
            if (initType && var->valueType) {
                if (!TypeChecker::isAssignable(initType, var->valueType, ctx)) {
                    ctx.error(var->init->loc, DiagCode::E2001,
                              "cannot initialize variable '", ctx.pool.lookup(var->name),
                              "' with value of different type");
                }
            }
        } else if (var->keyword == DeclKeyword::Const) {
            ctx.error(var->loc, DiagCode::E2001,
                      "const variable '", ctx.pool.lookup(var->name),
                      "' must be initialized");
        } else if (!TypeChecker::isNullable(var->valueType, *ctx.typeResolver)) {
            ctx.error(var->loc, DiagCode::E2001,
                      "non-nullable variable '", ctx.pool.lookup(var->name),
                      "' must be initialized");
        }
    }
    // FuncDeclAST local functions are checked elsewhere
}