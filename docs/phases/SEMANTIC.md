# LUC Semantic Analysis Phase

The Semantic Analysis phase in the LUC compiler is responsible for ensuring the program adheres to the language's structural and type rules. It transforms the raw Abstract Syntax Tree (AST) produced by the parser into a fully validated and annotated tree ready for Code Generation.

## General Flow

The semantic phase is divided into four distinct sub-phases, orchestrated by the `SemanticAnalyzer`.

### Phase 1: Symbol Collection (`SemanticCollector.cpp`)
Performs a fast first pass to discover all top-level declarations (Structs, Enums, Traits, Functions) across all files in a package.
- **Goal**: Populate the `SymbolTable` with type and function names so they can be referenced before their definition (allowing mutual recursion).

### Phase 2: Imports and Aliases (`SemanticCollector.cpp`)
Resolves `use` declarations (imports) and type aliases.
- **Goal**: Ensure all external references and local names are mapped to their true semantic definitions.

### Phase 3: Type Resolution and Check (`SemanticDecl.cpp`, `SemanticExpr.cpp`, `SemanticStmt.cpp`)
The "heavy lifting" phase. Performs full, recursive validation of all code.
- **3a (Decls)**: Validates that variable types exist and constant rules are followed.
- **3b (Exprs)**: Infers types for every expression and writes the `resolvedType` field directly onto AST nodes.
- **3c (Stmts)**: Manages scope depths, validates control flow (`break`/`continue` location), and handles type narrowing (`if x is T`).

### Phase 4: Final Annotation (`Annotator.cpp`)
A post-order walk of the tree that stamps final semantic metadata for the codegen.
- **Goal**: Propagate `isConst` flags bottom-up and reinforce `isBehaviorMember` markings.

> **NOTE**: To understand the code, start with SemanticAnalyzer.hpp` and `SemanticAnalyzer.cpp`.

---

## File Responsibilities

| File | Responsibility |
| :--- | :--- |
| **`SemanticAnalyzer.hpp/cpp`** | The "Driver". Orchestrates the sequence of phases and manages the high-level lifetime of symbols and diagnostics. |
| **`SymbolTable.hpp/cpp`** | Manages the stack of scopes. Handles name lookup, shadowing rules, and ensures names don't leak between blocks. |
| **`SemanticCollector.hpp/cpp`** | Implements Phase 1 and 2. Scans for top-level symbols and resolves package-wide visibility. |
| **`SemanticDecl.cpp`** | Logic for validating declarations (variables, functions, structs, enums). |
| **`SemanticExpr.cpp`** | Logic for expression type-checking, inference, and binary operator compatibility. |
| **`SemanticStmt.cpp`** | Logic for statement validation, block scoping, and control flow constraints. |
| **`Annotator.cpp`** | Implements Phase 4. Attaches `isConst` and other codegen-critical metadata to the AST via a post-order visitor walk. |
| **`TypeResolver.hpp/cpp`** | Converts `TypeAST` (syntactic representation of types) into actual semantic type objects. |
| **`TypeChecker.hpp/cpp`** | Provides utilities for checking type compatibility (equivalence, subtyping, union narrowing). |
| **`SemanticSymbol.hpp`** | Defines the `Symbol` structure—the glue that links an identifier name to its actual AST declaration and metadata. |

---

## The ASTVisitor Pattern

The `ASTVisitor` (defined in `BaseAST.hpp`) is the architectural backbone of the LUC compiler's multi-pass design. It allows the compiler to separate the **structure** of the code (the AST nodes) from the **logic** that operates on it (semantic checks, codegen).

### Relationship: Node ↔ Visitor
Every concrete AST node (e.g., `VarDeclAST`, `BinaryExprAST`) implements the `accept(ASTVisitor&)` method. This is a classic implementation of the **Visitor Pattern** also known as **Double Dispatch**:

1.  **Dispatch 1**: You call `node->accept(visitor)`. Since `accept` is virtual, C++ selects the implementation on the concrete class (e.g., `BinaryExprAST`).
2.  **Dispatch 2**: The implementation `visitor.visit(*this)` is called. Since `visit` is overloaded for every node type, C++ selects the specific method for that node type (e.g., `visit(BinaryExprAST&)`).

### Relationship: Semantic Phase ↔ Visitor
The semantic phase uses visitors to perform deep, recursive walks of the tree.

*   **Static Dispatch**: Each visitor only overrides the `visit()` methods it actually cares about. The base `ASTVisitor` provides no-op implementations for every node type by default.
*   **Traversal Control**: Unlike generic walkers, an `ASTVisitor` implementation in LUC (like the `Annotator`) controls its own traversal. It decides whether to walk children first (Post-Order) or last (Pre-Order) by calling `node->child->accept(*this)`.
*   **State Management**: Because a visitor is a class, it can maintain state (like a reference to the `SymbolTable` or a diagnostic counter) as it moves through the tree.

### Core Architecture Files

| File | Role in Visitor Pattern |
| :--- | :--- |
| **`BaseAST.hpp`** | Defines the `ASTVisitor` interface with all `visit` overloads and the `accept` virtual hook. |
| **`Expr/Stmt/DeclAST.hpp`** | Every concrete struct in these files implements `accept` to perform the callback to the visitor. |
| **`SemanticExpr.cpp`** | Uses a visitor-like approach (though often specialized) for expression type inference. |
| **`Annotator.cpp`** | The cleanest example of an `ASTVisitor` implementation, performing a post-order walk to propogate constants. |

### Why use a Visitor?
1.  **Cleaner AST**: AST nodes stay as "dumb" data structures. They don't need to know anything about type checking or machine code.
2.  **Extensibility**: Adding a new pass (e.g., a Linting pass or a Linter) only requires creating a new `ASTVisitor` subclass; the AST headers never need to change.
3.  **Encapsulation**: The complex logic for Phase 3 and Phase 4 is encapsulated in their respective files rather than being scattered across dozens of AST node files.
