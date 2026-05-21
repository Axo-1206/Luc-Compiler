# Luc Compiler

Luc is a modern systems programming language designed for performance, clarity, and graphics programming (Vulkan). It combines functional and procedural paradigms with a strong focus on composition and explicit semantics.

## Table of Contents
- [Project Overview](#project-overview)
    - [Project Identity](#project-identity)
    - [Design Philosophy](#design-philosophy)
    - [Codebase Structure](#codebase-structure)
- [Language Grammar](docs/LUC_GRAMMAR.md)

---

## Project Overview

### Project Identity

| Field | Value |
|---|---|
| Language name | `luc` |
| Compiler written in | C++ |
| Compiler backend | LLVM 18.1.6 |
| Execution model | JIT (Just-In-Time), cross-platform and AOT |
| Primary use case | Systems + graphics programming (Vulkan) |
| Build system | CMake + vcpkg (`x64-windows`, 2024-04-23) |

### Design Philosophy

Luc follows a **functional / procedural / composite / module** paradigm.

- **No classes or inheritance** — OOP-style hierarchies are intentionally absent and rejected at the semantic level.
- **Struct-Impl** — Go-inspired primary data and component structure; acts as typed composites (struct + behavior, no class semantics).
- **Module system** — Go-inspired file layout; central to code organization; modules resolve at semantic time.
- **First-class functions** — Composition over inheritance throughout.
- **Vulkan-programming** — Designed with graphics programming as a primary target.
- **Game-development** — This language is used for Lucid game engine project.

### Codebase Structure

A high-level map of the files and directories in this repository:

* [CMakeLists.txt](CMakeLists.txt) — Build configuration file.
* [LICENSE](LICENSE) — Project license.
* [README.md](README.md) — Main documentation and overview.
* 📁 [docs/](docs) — Technical specifications and documentation.
  * [LUC_GRAMMAR.md](docs/LUC_GRAMMAR.md) — Comprehensive language grammar and syntax rules.
  * [LUC_DIAGNOSTIC_CODES.md](docs/LUC_DIAGNOSTIC_CODES.md) — Compiler error and warning diagnostic codes.
  * 📁 [designs/](docs/designs)
    * [COMPILER_DIRECTIVE.md](docs/designs/COMPILER_DIRECTIVE.md) — Compiler directives specification.
  * 📁 [std_libraries/](docs/std_libraries)
    * [LUC_IO.md](docs/std_libraries/LUC_IO.md) — I/O standard library documentation.
    * [LUC_REGEX.md](docs/std_libraries/LUC_REGEX.md) — Regular expressions standard library.
* 📁 [src/](src) — Luc compiler source code.
  * [main.cpp](src/main.cpp) — Compiler entry point and driver.
  * [Tokens.hpp](src/Tokens.hpp) — Token definitions for the lexer and parser.
  * 📁 [lexer/](src/lexer) — Lexical analysis implementation.
    * [Lexer.hpp](src/lexer/Lexer.hpp) / [Lexer.cpp](src/lexer/Lexer.cpp)
  * 📁 [ast/](src/ast) — Abstract Syntax Tree node representations.
    * [BaseAST.hpp](src/ast/BaseAST.hpp) — Base AST classes and visitor patterns.
    * [DeclAST.hpp](src/ast/DeclAST.hpp) — AST nodes for declarations.
    * [ExprAST.hpp](src/ast/ExprAST.hpp) — AST nodes for expressions.
    * [StmtAST.hpp](src/ast/StmtAST.hpp) — AST nodes for statements.
    * [TypeAST.hpp](src/ast/TypeAST.hpp) — AST nodes for types.
    * 📁 [support/](src/ast/support) — Core utility structures for AST building (e.g. `ASTArena.hpp`, `StringPool.cpp`, `InternedString.hpp`).
  * 📁 [parser/](src/parser) — Parsing implementation to build the AST.
    * [Parser.hpp](src/parser/Parser.hpp) / [Parser.cpp](src/parser/Parser.cpp)
    * [ParserDecl.cpp](src/parser/ParserDecl.cpp)
    * [ParserExpr.cpp](src/parser/ParserExpr.cpp)
    * [ParserStmt.cpp](src/parser/ParserStmt.cpp)
    * [ParserType.cpp](src/parser/ParserType.cpp)
  * 📁 [semantic/](src/semantic) — Semantic analysis, type resolution, and validation.
    * [Annotator.cpp](src/semantic/Annotator.cpp) — AST annotation phase.
    * [SemanticAnalyzer.cpp](src/semantic/SemanticAnalyzer.cpp) — Semantic analysis driver.
    * [SemanticCollector.cpp](src/semantic/SemanticCollector.cpp) — Collects types and symbols (phase 1 & 2).
    * [SemanticDecl.cpp](src/semantic/SemanticDecl.cpp) — Processes declarations (phase 3).
    * [SemanticExpr.cpp](src/semantic/SemanticExpr.cpp) — Processes expressions (phase 3).
    * [SemanticStmt.cpp](src/semantic/SemanticStmt.cpp) — Processes statements (phase 3).
    * [SemanticUtils.cpp](src/semantic/SemanticUtils.cpp) — Auxiliary semantic utilities.
    * [SymbolTable.cpp](src/semantic/SymbolTable.cpp) — Symbol environment and scope management.
    * [TypeChecker.cpp](src/semantic/TypeChecker.cpp) — Verifies type compatibility and safety rules.
    * [TypeResolver.cpp](src/semantic/TypeResolver.cpp) — Resolves variable, function, and composite types.
    * 📁 [header/](src/semantic/header) — Directory containing semantic analysis header files.
  * 📁 [codegen/](src/codegen) — Code generation using the LLVM backend.
    * [CodeGen.hpp](src/codegen/CodeGen.hpp) / [CodeGen.cpp](src/codegen/CodeGen.cpp)
    * [luc_runtime.c](src/codegen/luc_runtime.c) — Runtime C library linked with compiled code.
  * 📁 [registry/](src/registry) — Global registries for builtins, qualifiers, attributes, and intrinsics.
    * [AttributeRegistry.cpp](src/registry/AttributeRegistry.cpp) / [AttributeRegistry.hpp](src/registry/AttributeRegistry.hpp)
    * [BuiltinMethodRegistry.cpp](src/registry/BuiltinMethodRegistry.cpp) / [BuiltinMethodRegistry.hpp](src/registry/BuiltinMethodRegistry.hpp)
    * [IntrinsicRegistry.cpp](src/registry/IntrinsicRegistry.cpp) / [IntrinsicRegistry.hpp](src/registry/IntrinsicRegistry.hpp)
    * [QualifierRegistry.cpp](src/registry/QualifierRegistry.cpp) / [QualifierRegistry.hpp](src/registry/QualifierRegistry.hpp)
  * 📁 [diagnostics/](src/diagnostics) — Custom compiler error and warning logs reporting.
  * 📁 [debug/](src/debug) — Debugging features, including [ASTDumper.cpp](src/debug/ASTDumper.cpp).
* 📁 [tests/](tests) — Test suites validating compiling and execution pipeline.
* 📁 [language_support/](language_support) — Editor and IDE syntax highlighting support.
* 📁 [archive/](archive) — Deprecated or historical plans, code, and documentation.

---

## Language Grammar

Detailed information about the language syntax, types, and rules can be found in the [LUC_GRAMMAR.md](docs/LUC_GRAMMAR.md) file.
