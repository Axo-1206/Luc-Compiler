/**
 * @file LiteralChecker.cpp
 * @brief Implementation of literal expression checking.
 * 
 * Handles: LiteralExprAST, ArrayLiteralExprAST, StructLiteralExprAST
 */

#include "ExprChecker.hpp"
#include "semantic/checker/TypeChecker.hpp"
#include "debug/DebugMacros.hpp"

TypeAST* checkLiteralExpr(LiteralExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkLiteralExpr: kind=" << static_cast<int>(expr->kind));
    
    switch (expr->kind) {
        case LiteralKind::Int:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Int);
        case LiteralKind::Float:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Float);
        case LiteralKind::String:
        case LiteralKind::RawString:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::String);
        case LiteralKind::Char:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Char);
        case LiteralKind::True:
        case LiteralKind::False:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Bool);
        case LiteralKind::Nil:
            // nil is a special value; its type is determined by context
            return nullptr;
        case LiteralKind::Hex:
        case LiteralKind::Binary:
            return ctx.arena.make<PrimitiveTypeAST>(PrimitiveKind::Int);
        default:
            ctx.error(expr->loc, DiagCode::E2001, "unknown literal type");
            return nullptr;
    }
}

TypeAST* checkArrayLiteralExpr(ArrayLiteralExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkArrayLiteralExpr: " << expr->elements.size() << " elements");
    
    if (expr->elements.empty()) {
        // Empty array literal – type must be inferred from context
        return nullptr;
    }
    
    // Check all elements and find common type
    TypeAST* commonType = nullptr;
    
    for (auto* elem : expr->elements) {
        TypeAST* elemType = checkExpr(elem, ctx);
        if (!elemType) return nullptr;
        
        if (!commonType) {
            commonType = elemType;
        } else {
            commonType = TypeChecker::unify(commonType, elemType, ctx);
            if (!commonType) {
                ctx.error(elem->loc, DiagCode::E2001, 
                          "array elements have incompatible types");
                return nullptr;
            }
        }
    }
    
    return ctx.arena.make<ArrayTypeAST>(ArrayKind::Slice, 0, commonType);
}

TypeAST* checkStructLiteralExpr(StructLiteralExprAST* expr, SemanticContext& ctx) {
    LUC_LOG_SEMANTIC_EXTREME("checkStructLiteralExpr: " << ctx.pool.lookup(expr->typeName));
    
    // Look up the struct type
    TypeDeclAST* typeDecl = ctx.scope.lookupType(expr->typeName);
    if (!typeDecl) {
        ctx.error(expr->loc, DiagCode::E2001, 
                  "undefined struct type '", ctx.pool.lookup(expr->typeName), "'");
        return nullptr;
    }
    
    auto* structDecl = typeDecl->as<StructDeclAST>();
    if (!structDecl) {
        ctx.error(expr->loc, DiagCode::E2001, 
                  "'", ctx.pool.lookup(expr->typeName), "' is not a struct");
        return nullptr;
    }
    
    // Check field initializers
    for (auto* init : expr->inits) {
        if (!init) continue;
        
        bool found = false;
        for (auto* field : structDecl->fields) {
            if (field->name == init->name) {
                found = true;
                
                TypeAST* initType = checkExpr(init->value, ctx);
                if (!initType) return nullptr;
                
                if (!TypeChecker::isAssignable(initType, field->valueType, ctx)) {
                    ctx.error(init->value->loc, DiagCode::E2001,
                              "cannot assign to field '", ctx.pool.lookup(field->name), 
                              "' of incompatible type");
                    return nullptr;
                }
                break;
            }
        }
        
        if (!found) {
            ctx.error(init->loc, DiagCode::E2001,
                      "struct '", ctx.pool.lookup(expr->typeName), 
                      "' has no field named '", ctx.pool.lookup(init->name), "'");
            return nullptr;
        }
    }
    
    expr->instantiatedType = structDecl->selfType;
    return structDecl->selfType;
}