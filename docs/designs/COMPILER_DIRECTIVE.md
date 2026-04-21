# Luc Design: The `@` Compiler Directive

This document summarizes the role of the `@` symbol in Luc, replacing the legacy `extern` blocks and providing a unified interface for compiler authority, FFI, and intrinsics.

## 1. Core Philosophy
The `@` symbol represents **Compiler Authority**. It is used for tasks that cannot be expressed through standard procedural logic. It is zero-cost at runtime and acts as a bridge to C-interop and LLVM backends.

## 2. Syntax Specifications

### A. Attributes (Top-Down)
Attributes are metadata attached to declarations (`struct`, `const`, `let`, ...). They are processed during the **Semantic Phase** and stored in the **Symbol Table**.

**Grammar:**
```ebnf
attribute      ::= "@" identifier [ "(" attr_args ")" ]
attr_args      ::= attr_value { "," attr_value }
attr_value     ::= string_literal | integer_literal | type_identifier
declaration    ::= [ attribute ]* ( func_decl | struct_decl | var_decl )
```

**Common Use Cases:**
* `@extern("name")`: Replaces `extern` blocks. Maps a Luc function to a C/OS symbol.
* `@inline` / `@noinline`: Directs the LLVM optimizer.
* `@packed`: Removes padding from a `struct` for binary compatibility.
* `@deprecated("message")`: Triggers a compiler warning on use.

### B. Intrinsics (Inline)
Intrinsics are "magic functions" implemented directly by the compiler. They appear as expressions within function bodies.

**Grammar:**
```ebnf
primary_expr   ::= intrinsic_call | identifier | literal | "(" expr ")"
intrinsic_call ::= "@" identifier "(" [ argument_list ] ")"
```

**Common Use Cases:**
* `@sizeof(T)`: Returns the byte-size of a type.
* `@memcpy(dest, src, len)`: Maps to `llvm.memcpy`.
* `@sqrt(x)`: Maps to hardware-accelerated square root.

## 3. Implementation Logic



### The Linker vs. The Compiler
* **FFI (@extern):** The compiler marks the symbol as external. The **Linker** resolves it automatically at build time. No constant expansion of C++ code is required once `@extern` is implemented.
* **Directives (@inline, @sqrt):** These require **Manual Expansion** in the C++ backend. Each directive must be explicitly mapped to an LLVM attribute or intrinsic ID.

## 4. Why this benefits Luc
1. **Removes Keyword Pollution:** `inline` and `packed` don't need to be reserved words, avoiding conflicts with C libraries.
2. **C-Coexistence:** It follows the Zig philosophy of "no hidden magic." If you see `@`, you know the compiler is interfering.
3. **Flat Grammar:** Eliminating `extern` blocks keeps the parser simpler and the module structure flat, aligning with Luc's procedural goals.

***

### Summary Table for Backend Development

| Feature | Logic Location | C++ Update Required? | Primary Backend Target |
| :--- | :--- | :--- | :--- |
| **@extern** | Symbol Table | No (Standard Logic) | System Linker |
| **@inline** | Codegen | Yes (Function Flags) | LLVM Optimizer |
| **@packed** | Semantic Phase | Yes (Type Layout) | LLVM Data Layout |
| **@intrinsics** | Codegen | Yes (Intrinsic Mapping) | LLVM IR |



# Technical Challenges: The `@` Directive Migration

## 1. Replacing the `extern` Block
Moving from a block-based `extern "C" { ... }` to a per-declaration `@extern` attribute solves grammar nesting but introduces two primary issues:

### A. Context Loss (The "C" linkage problem)
In a block, you specify the language (e.g., `"C"`) once for twenty functions. With attributes, you must ensure the compiler knows the **Calling Convention** for every single external symbol.
* **The Problem:** If you forget to specify that a function uses C-linkage, the LLVM backend might mangle the name or use the wrong stack cleanup logic.
* **The Solution:** Default `@extern` to the "C" calling convention unless a second parameter is provided (e.g., `@extern("func", "stdcall")`).

### B. Header Bloat vs. Granularity
* **The Problem:** In C, headers are often included as a whole. In Luc, you are now declaring external functions one by one. This can lead to "copy-paste" errors where a function signature in Luc doesn't match the actual C library.
* **The Solution:** Use the `@` syntax to point to external definitions (e.g., `@import_c("stdio.h")`) if you want to automate the mapping, though manual `@extern` is safer for strict "Better C" control.



## 2. Flexible Parameter Types (Beyond Strings)
By allowing identifiers, integers, and types inside `@attr(...)`, you increase the complexity of your **Attribute Parser**.

### A. The "Type-Before-Definition" Problem
* **The Problem:** If you use `@sizeof(UserStruct)`, but `UserStruct` is defined at the bottom of the file, your compiler might fail during the first pass because it doesn't know the size of the type yet.
* **The Solution:** Attributes that take **Types** as parameters must be resolved in a "Late Semantic" pass, after all top-level types have been registered in the Symbol Table.

### B. Expression Ambiguity
* **The Problem:** Is `@attr(x + 5)` allowed? If you allow math expressions inside attributes, the attribute logic becomes as complex as the main function logic.
* **The Solution:** Restrict attribute parameters to **Literals** (Strings, Ints, Bools) and **Type Identifiers**. Do not allow runtime math inside a compile-time attribute.

## 3. Backend Mapping Expansion
The biggest challenge is ensuring that your C++ compiler code stays maintainable as you add more "magic" `@` commands.

* **The Problem:** Every new intrinsic like `@sqrt` or `@memcpy` requires a manual entry in your C++ `switch` statement in the Codegen phase.
* **The Solution:** Implement an **Intrinsic Registry**. Instead of hardcoding logic, use a table that maps the Luc name to the LLVM Intrinsic ID.



| Implementation Problem | Phase Impacted | Risk Level | Mitigation |
| :--- | :--- | :--- | :--- |
| **Name Mangling** | Codegen | High | Force "C" Linkage as default for `@extern`. |
| **Circular Type Refs** | Semantic | Medium | Resolve `@` type parameters in a second pass. |
| **Parameter Overload** | Parser | Low | Limit parameters to Literals and Identifiers only. |
| **Codegen Bloat** | C++ Backend | Medium | Use a Registry/Map for LLVM Intrinsics. |

By addressing these "problems" early, you ensure that Luc remains a stable systems language that is easy to link with C/Vulkan while maintaining its own unique procedural/functional hybrid identity.




---

# 5. Advanced Backend Integration

To fully utilize the power of the LLVM backend and ensure seamless OS/Linker interoperability, the following technical specifications must be integrated into the Luc compiler logic.

## A. LLVM Calling Conventions & Attributes
Luc leverages the LLVM `CallingConv` enum to ensure compatibility with foreign functions.
*   **Default (C)**: Luc defaults to the target-specific C calling convention. On x86_64, this follows the Microsoft x64 ABI or the System V AMD64 ABI.
*   **Explicit CC**: Via `@extern("name", "stdcall")`, Luc can instruct the LLVM backend to use specific conventions like `CallingConv::X86_StdCall`.
*   **Optimization Hints**: Attributes like `@inline` and `@noinline` map directly to `llvm::Attribute::AlwaysInline` and `llvm::Attribute::NoInline` on the `llvm::Function` object.

## B. OS & ABI Constraints
For direct OS interaction (syscalls) and FFI safety:
*   **Stack Alignment**: The backend must ensure the stack is aligned to 16 bytes before calling a function marked with `@extern` on most modern Unix and Windows systems.
*   **Name Mangling**: `@extern` functions skip the Luc mangler, allowing direct linkage to C symbols. This is critical for OS-specific globals like `errno` or `environ`.

## C. Linker Resolution & Visibility
*   **Symbol Visibility**: Future `@visibility("hidden")` or `@visibility("default")` attributes will map to ELF/Mach-O visibility levels, allowing Luc libraries to hide implementation details from the dynamic linker.
*   **Search Paths**: Extensions to the `@` directive (e.g., `@link_lib("libname")`) can be used to pass hints to the system linker (e.g., `-l` flags) during the final binary emission.

---

# 6. Implementation Reference (C++ Backend)

Developing new features for the `@` directive requires updating specific semantic checkpoints in the Luc source code.

## A. Attribute Validation: `checkAttributes`
*   **Location**: `src/semantic/SemanticDecl.cpp` (Approx. line 50)
*   **Role**: This function validates the `@` attribute lists attached to declarations (`struct`, `var`, `func`).
*   **Expansion Note**: When adding a new top-level attribute (e.g., `@section`), this function must be updated to validate high-level syntax and store the metadata in the `Symbol` or `DeclAST` node.

## B. Intrinsic Validation: `checkIntrinsicCallExpr`
*   **Location**: `src/semantic/SemanticExpr.cpp` (Approx. line 1419)
*   **Role**: Handles inline "magic" functions like `@sizeof` or `@bitcast`.
*   **Expansion Note**: While this function is registry-driven via `IntrinsicRegistry.hpp`, unique validation logic (like verifying bit-widths for `@bitcast`) occurs here. Any new intrinsic requiring special type-checking must be integrated into this switch/registry logic.

---

> [!NOTE]
> All future expansions of the `@` directive should prioritize **Aesthetics and Low Overhead**. If a feature can be expressed through a standard library wrapper, it should not be an intrinsic.
