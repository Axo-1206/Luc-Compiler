# Luc — Project Overview

> **Scope of this file:** Project identity, architecture, pipeline status, and build environment.
> Grammar, syntax, and code examples are documented separately in `LUC_GRAMMAR.md`, `LUC_EXAMPLES.md` and standard library like `LUC_IO.md` and `LUC_ERROR.md`.

---

## Project Identity

| Field               | Value                                      |
| ------------------- | ------------------------------------------ |
| Language name       | `luc`                                      |
| Compiler written in | C++                                        |
| Compiler backend    | LLVM 18.1.6                                |
| Execution model     | JIT (Just-In-Time), cross-platform and AOT |
| Primary use case    | Systems + graphics programming (Vulkan)    |
| Build system        | CMake + vcpkg (`x64-windows`, 2024-04-23)  |

---

## Design Philosophy

Luc follows a **functional / procedural / composite / module** paradigm.

- **No classes or inheritance** — OOP-style hierarchies are intentionally absent and rejected at the semantic level
- **Struct-Impl** (Go-inspired) — primary data and component structure; act as typed composites (struct + behavior, no class semantics)
- **Module system** (Go-inspired file layout) — central to code organization; modules resolve at semantic time
- **First-class functions** — composition over inheritance throughout
- **Vulkan-programming** — the language is designed with graphics programming as a primary target
- **Game-development** — the language support an io library that support binding event to system input(keycode, mouse, touch, ...) and math library for game development

---

## Codebase Structure

---

## Compiler Pipeline

```
Source (.luc)
    │
    ▼
[ Lexer ]           
    │
    ▼
[ AST ]             
    │
    ▼
[ Parser ]          
    │
    ▼
[ Semantic ]            
    │
    ▼
[ IR / LLVM ]           
    │
    ▼
[ JIT / Codegen ]   
```