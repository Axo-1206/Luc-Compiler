/**
 * @file StmtChecker.cpp
 * @brief Implementation of the statement dispatcher.
 * 
 * This file contains only the dispatch logic. All statement-specific
 * validation is implemented in their respective .cpp files.
 */

#include "StmtChecker.hpp"
#include "debug/DebugMacros.hpp"
#include "debug/DebugUtils.hpp"

void checkStmt(StmtAST* stmt, SemanticContext& ctx, TypeAST* expectedReturn) {
    if (!stmt) return;
    
    LUC_LOG_SEMANTIC_EXTREME("checkStmt: kind=" << LucDebug::kindToString(stmt->kind));
    
    switch (stmt->kind) {
        case ASTKind::BlockStmt:
            checkBlockStmt(stmt->as<BlockStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::DeclStmt:
            checkDeclStmt(stmt->as<DeclStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::ExprStmt:
            checkExprStmt(stmt->as<ExprStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::IfStmt:
            checkIfStmt(stmt->as<IfStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::SwitchStmt:
            checkSwitchStmt(stmt->as<SwitchStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::ForStmt:
            checkForStmt(stmt->as<ForStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::WhileStmt:
            checkWhileStmt(stmt->as<WhileStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::DoWhileStmt:
            checkDoWhileStmt(stmt->as<DoWhileStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::ReturnStmt:
            checkReturnStmt(stmt->as<ReturnStmtAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::BreakStmt:
            checkBreakStmt(stmt->as<BreakStmtAST>(), ctx);
            break;
            
        case ASTKind::ContinueStmt:
            checkContinueStmt(stmt->as<ContinueStmtAST>(), ctx);
            break;
            
        case ASTKind::MultiVarDecl:
            checkMultiVarDecl(stmt->as<MultiVarDeclAST>(), ctx, expectedReturn);
            break;
            
        case ASTKind::MultiAssignStmt:
            checkMultiAssignStmt(stmt->as<MultiAssignStmtAST>(), ctx, expectedReturn);
            break;
            
        default:
            LUC_LOG_SEMANTIC("checkStmt: unhandled statement kind: "
                             << LucDebug::kindToString(stmt->kind));
            break;
    }
}