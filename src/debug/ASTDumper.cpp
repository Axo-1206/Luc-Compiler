#include "ASTDumper.hpp"
#include "DebugUtils.hpp"
#include "ast/DeclAST.hpp"
#include "ast/ExprAST.hpp"
#include "ast/StmtAST.hpp"
#include "ast/TypeAST.hpp"
#include <sstream>

namespace LucDebug {

ASTDumper::ASTDumper(int verb) : verbosity(verb), indentLevel(0) {}

void ASTDumper::indent() {
    for(int i=0; i<indentLevel; ++i) {
        out += "\t";
    }
}

void ASTDumper::printHeader(const BaseAST& node, const std::string& nodeName) {
    if (verbosity == 0) { // MINIMAL
        out += nodeName + "\n";
    } else if (verbosity == 1) { // NORMAL
        out += nodeName + " at line " + std::to_string(node.loc.line) + ", col " + std::to_string(node.loc.column) + "\n";
    } else if (verbosity >= 2) { // VERBOSE & EXTREME
        out += nodeName + " at line " + std::to_string(node.loc.line) + ", col " + std::to_string(node.loc.column) + "\n";
        indent(); out += "\t\tkind: " + kindToString(node.kind) + "\n";
        indent(); out += std::string("\t\ttype: ") + (node.resolvedType ? "resolved" : "<unresolved>") + "\n";
    }
}

// ── Root ──────────────────────────────────────────────────────────────────
void ASTDumper::visit(ProgramAST& node) {
    printHeader(node, "\tProgramAST");
    if (verbosity >= 3) {
        indent(); out += "\t\tpackageName: '" + node.packageName + "'\n";
        indent(); out += "\t\tfilePath: '" + node.filePath + "'\n";
        indent(); out += "\t\tdecls (count): " + std::to_string(node.decls.size()) + "\n";
        
        // Recurse into children
        indentLevel++;
        for (const auto& decl : node.decls) {
            if (decl) {
                indent();
                decl->accept(*this);
            }
        }
        indentLevel--;
    }
}

// Macro to provide a generic fallback for all other nodes
#define GENERIC_VISIT(NodeType) \
    void ASTDumper::visit(NodeType& node) { \
        printHeader(node, #NodeType); \
    }

// ── Type nodes ────────────────────────────────────────────────────────────
GENERIC_VISIT(PrimitiveTypeAST)
GENERIC_VISIT(NamedTypeAST)
GENERIC_VISIT(NullableTypeAST)
GENERIC_VISIT(FixedArrayTypeAST)
GENERIC_VISIT(SliceTypeAST)
GENERIC_VISIT(DynamicArrayTypeAST)
GENERIC_VISIT(RefTypeAST)
GENERIC_VISIT(PtrTypeAST)
GENERIC_VISIT(FuncTypeAST)

// ── Declaration nodes ─────────────────────────────────────────────────────
GENERIC_VISIT(PackageDeclAST)
GENERIC_VISIT(UseDeclAST)
GENERIC_VISIT(VarDeclAST)
GENERIC_VISIT(FuncDeclAST)
GENERIC_VISIT(StructDeclAST)
GENERIC_VISIT(FieldDeclAST)
GENERIC_VISIT(EnumDeclAST)
GENERIC_VISIT(EnumVariantAST)
GENERIC_VISIT(TraitMethodAST)
GENERIC_VISIT(TraitDeclAST)
GENERIC_VISIT(ImplDeclAST)
GENERIC_VISIT(MethodDeclAST)
GENERIC_VISIT(FromDeclAST)
GENERIC_VISIT(FromEntryAST)
GENERIC_VISIT(TypeAliasDeclAST)
GENERIC_VISIT(ParamAST)
GENERIC_VISIT(GenericParamAST)

// ── Expression nodes ──────────────────────────────────────────────────────
GENERIC_VISIT(LiteralExprAST)
GENERIC_VISIT(IdentifierExprAST)
GENERIC_VISIT(ArrayLiteralExprAST)
GENERIC_VISIT(StructLiteralExprAST)
GENERIC_VISIT(BinaryExprAST)
GENERIC_VISIT(UnaryExprAST)
GENERIC_VISIT(CallExprAST)
GENERIC_VISIT(IndexExprAST)
GENERIC_VISIT(FieldAccessExprAST)
GENERIC_VISIT(BehaviorAccessExprAST)
GENERIC_VISIT(NullableChainExprAST)
GENERIC_VISIT(AssignExprAST)
GENERIC_VISIT(IsExprAST)
GENERIC_VISIT(PipelineExprAST)
GENERIC_VISIT(ComposeExprAST)
GENERIC_VISIT(AnonFuncExprAST)
GENERIC_VISIT(AwaitExprAST)
GENERIC_VISIT(MatchExprAST)
GENERIC_VISIT(IfExprAST)
GENERIC_VISIT(RangeExprAST)
GENERIC_VISIT(TypeConvExprAST)

// ── Pattern nodes ─────────────────────────────────────────────────────────
GENERIC_VISIT(BindPatternAST)
GENERIC_VISIT(WildcardPatternAST)
GENERIC_VISIT(TypePatternAST)
GENERIC_VISIT(StructPatternAST)
GENERIC_VISIT(MatchArmAST)
GENERIC_VISIT(DefaultArmAST)

// ── Statement nodes ───────────────────────────────────────────────────────
GENERIC_VISIT(BlockStmtAST)
GENERIC_VISIT(ExprStmtAST)
GENERIC_VISIT(DeclStmtAST)
GENERIC_VISIT(IfStmtAST)
GENERIC_VISIT(SwitchStmtAST)
GENERIC_VISIT(ForStmtAST)
GENERIC_VISIT(WhileStmtAST)
GENERIC_VISIT(DoWhileStmtAST)
GENERIC_VISIT(ReturnStmtAST)
GENERIC_VISIT(BreakStmtAST)
GENERIC_VISIT(ContinueStmtAST)
GENERIC_VISIT(ParallelForStmtAST)
GENERIC_VISIT(ParallelBlockStmtAST)

// ── Compiler Directive nodes (@) ──────────────────────────────────────────
GENERIC_VISIT(AttributeAST)
GENERIC_VISIT(IntrinsicCallExprAST)

} // namespace LucDebug
