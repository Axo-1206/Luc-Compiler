Here is the **final, comprehensive `CODEGEN.md`** that merges the original plan, the later full‑grammar expansion, and the new decision to remove native union types (replacing them with `any` + `is`). All language features from `LUC_GRAMMAR.md` are covered, and no union type lowering remains.

```markdown
# Luc Compiler – Code Generation Plan (Final, Full Grammar Coverage)

> **Status:** Approved for implementation  
> **Last updated:** 2026‑05‑01  
> **Related docs:** `LUC_GRAMMAR.md`, `LUC_PROJECT_OVERVIEW.md`  
> **Key decision:** Native union types removed – use `any` + type assertion (`is`) instead.

This document describes the **complete code generation pipeline** for the Luc compiler, from AST to executable code (JIT or AOT). It includes all features defined in the grammar, with **no union type support**.

---

## 1. Overall Architecture (unchanged)

```text
┌─────────────┐     ┌──────────────────┐     ┌─────────────┐
│  Luc Source │────►│ Semantic Analyser│────►│   CodeGen   │
└─────────────┘     └──────────────────┘     └──────┬──────┘
                                                     │
                                                     ▼
                              ┌──────────────────────────────────┐
                              │        CompilationDriver         │
                              │  (chooses JIT or AOT per @main)  │
                              └────────┬──────────────┬──────────┘
                                       │              │
                                       ▼              ▼
                           ┌──────────────┐    ┌──────────────┐
                           │  JIT Engine  │    │  AOT Compiler│
                           │  (ORCv2)     │    │  (MCJIT)     │
                           └──────┬───────┘    └──────┬───────┘
                                  │                   │
                                  │      .lbc bytecode│
                                  │      ┌────────────┘
                                  ▼      ▼
                           ┌──────────────────┐
                           │  Serializer /    │
                           │  Bytecode format │
                           └──────────────────┘
```

All code generation passes operate on LLVM IR. The same IR is reused for JIT and AOT.

---

## 2. Core Codegen Phases

### Phase 0: LLVM Context & Basic Setup
- Initialize `llvm::LLVMContext`, `llvm::Module`, `llvm::IRBuilder<>`.
- Set target triple, data layout, and platform‑specific calling conventions.
- Define helper functions for Luc → LLVM type mapping.

### Phase 1: Expressions (Literals, Arithmetic, Variables)
- Literals: integers, floats, strings (with escapes), raw strings, chars, hex/binary.
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `^` (power, right‑associative).
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`, `===` (reference equality).
- Logical: `and`, `or`, `not` (short‑circuit).
- Bitwise: `&&` (AND), `||` (OR), `~^` (XOR), `~` (NOT), `<<`, `>>` (integers only).
- Variable access (local, global, upvalues for closures).

### Phase 1.5: `any` Type Support (replaces union types)
- **Boxing** – any value can be placed into `any`:
  - Primitives, structs, enums, arrays, functions, references → heap‑allocated box.
- **Box representation** – runtime `struct any { void* data; TypeDescriptor* type; uint64_t flags; }`.
- **Unboxing** – via `is` operator (see Phase 2.5).
- **No union types** – `int | string` is **not** supported. Use `any` + type assertion instead.

### Phase 2: Statements & Control Flow
- Blocks, `if`‑`else` (statement), `while`, `for`‑`range`, `do`‑`while`, `return`, `break`, `continue`.
- **`switch` statement** – compile to jump table or if‑chain; cases can be values or ranges (`..`, `..<`); no fallthrough.
- **`match` expression** – lowered to nested `if`‑`else` or switch‑like dispatch with pattern matching. Supports:
  - Value patterns (`42`, `"ok"`)
  - Range patterns (`1..10`, `1..<10`)
  - Bind patterns (`n` with optional guard `if n > 0`)
  - Wildcard `_`
  - Struct destructuring (`Vec2 { x, y }`)
  - Type patterns with `is` (see Phase 2.5)
- Secondary values are packed into a small struct or returned via multiple registers.

### Phase 2.5: `is` Operator and Type Narrowing
- **`expr is Type`** – checks runtime type of an `any` box (or nullable, enum, etc.).
- **Type narrowing** – inside `if is` (statement) or `match` arm, the bound variable is treated as the target type.
- **LLVM lowering**:
  - For `any`: compare box’s `type` field with the target type’s descriptor.
  - For nullable: `is T` is false because `T?` is distinct from `T`. Use `== nil` for null check.
  - For enums: compare integer tag.
- **Dynamic dispatch** – no vtable; type checks are direct tag comparisons.
- **No union types** – the `is` operator is the primary way to handle heterogeneous values.

### Phase 3: Functions & Non‑Generic Types
- Function declarations (single parameter group, variadic `...T` lowered to slice or C varargs).
- Calls: normal, method calls, curried calls.
- Local `let` / `const` – constants are inlined by the constant folder.
- Structs (non‑generic) – LLVM struct types.
- **Struct field default values** – generate implicit initialiser that fills omitted fields.
- **Borrowed references `&T`** – LLVM pointer (non‑owning). Semantic analysis enforces borrowing rules.
- **Deep copy semantics** – owned structs: generate recursive copy of each owned field (clone for `[*]T`, `string`, nested structs). Borrowed fields copy the reference only.
- Type aliases – resolved during sema, no IR impact.
- Nullable types `T?` – represented as `{ T, i1 }` (value + validity flag) or via a runtime nullable handle. Unwrapping branches on flag and panics if invalid.

### Phase 3.5: Generics (Monomorphization)
- Generic structs, functions, impl blocks (syntax: `impl Vec2<T> : Drawable`).
- Monomorphization with caching and name mangling (e.g., `Vec2_int`).
- Trait constraints – resolved at monomorphisation time; no runtime dispatch.
- Generic `from` conversions are **not** allowed.

### Phase 4: Impl Methods, Traits, and Behaviours
- `impl` blocks → regular functions with explicit `self` parameter (or receiver via pointer).
- Method call `Type:method` – desugared to a function call.
- **Trait declarations** – stored as method signature sets; code generation for traits is metadata only (no vtable). Conformance checked semantically.
- **No default methods** – every trait method must be implemented.

### Phase 5: Type Conversions & Casting
- **Primitive conversions** (`int(x)`, `float(x)`, `string(x)`, etc.) → LLVM `sitofp`, `fptosi`, `uitofp`, `zext`, `trunc`, or runtime call for `string(x)`.
- **User‑defined conversions (`from` declaration)** – each `from` entry becomes a standalone function. Explicit `Target(expr)` calls it; implicit casts (assignments, argument passing, returns) also insert a call.
- **Implicit coercion** – inserted during expression codegen where required by sema.
- **Nullable unwrapping** – generate runtime panic (`__luc_panic`) when `nil` is unwrapped.
- **Null‑coalescing `??` operator** – compile to: evaluate LHS; if non‑nil, take unwrapped value; else evaluate RHS.

### Phase 6: Arrays
- Fixed `[N]T` – LLVM array type, inline allocation. Indexing via `extractvalue`/`insertvalue` or GEP.
- Slice `[]T` – `{ T* ptr, i64 len, i64 cap }` fat pointer; no ownership.
- Dynamic `[*]T` – heap‑owned buffer (runtime `__luc_array` handle). Methods (`.push`, `.pop`, `.len`, `.cap`, etc.) call runtime functions.
- **Concatenation `+`** – allocate new dynamic array and copy elements.
- **Range slicing `[start..end]` / `[start..<end]`** – produce slice fat pointer with bounds checking.
- **Out‑of‑bounds** – dynamic arrays return `nil` (programmer must handle); fixed/slice arrays panic.

### Phase 7: Advanced Expressions & Operators
- **Currying** – multiple parameter groups desugar to nested anonymous functions that capture previous arguments.
- **Closures** (anonymous functions with captured variables) – heap‑allocated environment struct. Capture rules:
  - Value types are copied.
  - Reference types (`&T`, `[]T`, `[*]T`) store the reference.
- **Pipeline operator `->`** – lower each step to function call, injecting upstream value as first argument (or discarding if step has no parameters). Argument pack `fn(args)!` resolved by inserting upstream before listed args.
  - **Error propagation**: if a step returns `Error` and the next step does not accept it, skip remaining steps and forward the error. Implemented with a hidden error flag.
- **Composition operator `+>`** – compile‑time function composition; validated at semantic level, generates a new function that calls left then right. No runtime overhead.
- **Nullable chain `.?`** – lower to: evaluate left side, if `nil` then result is `nil`, else perform field/method access.

### Phase 8: Async / Await (Coroutines)
- Transform `async` functions using LLVM coroutine intrinsics (`llvm.coro.*`).
- `await` expression suspends the coroutine and yields to the runtime scheduler.
- Runtime library (`AsyncRuntime.cpp`) provides event loop and task management.
- `await` is **not allowed** inside `parallel` scopes (semantic error).

### Phase 9: Parallel Execution
- `parallel for` – use `OpenMPIRBuilder` or a custom work‑stealing pool; iterate over range or collection.
- `parallel` block – each sub‑block submitted as a task to a thread pool; synchronise after all finish.
- **Restrictions** (semantic checks): no shared mutable state, no `await`, no `return`, no `break`/`continue` crossing parallel boundary.

### Phase 10: Dual Compilation – JIT & AOT
- `CompilationDriver` unified interface.
- JIT: ORCv2, AOT: MCJIT + object file emission.
- Bytecode `.lbc` serialisation (including metadata for generics, traits, modules).

### Phase 11: FFI & Intrinsics – Complete Set
#### 11.1 Attributes
| Attribute | LLVM mapping |
|-----------|---------------|
| `@extern("sym")` | `declare` with `dllimport`/`extern` – no body |
| `@inline` | `alwaysinline` |
| `@noinline` | `noinline` |
| `@packed` | `packed` LLVM struct layout |
| `@deprecated` | Emit `llvm.dbg` metadata (warning) |
| `@aot`/`@jit` | Decides compile path – no IR difference |

#### 11.2 Type & memory intrinsics
- `@sizeof(T)`, `@alignof(T)` → LLVM DataLayout queries (compile‑time constant).

#### 11.3 Floating‑point / math (complete)
- `@sqrt`, `@floor`, `@ceil`, `@round`, `@abs`, `@pow`, `@fma`, `@min`, `@max` → map to LLVM intrinsics (`llvm.sqrt.*`, `llvm.fma.*`, `select` for min/max).

#### 11.4 Bit manipulation (complete)
- `@clz`, `@ctz`, `@popcount`, `@bswap` → LLVM `llvm.ctlz.*`, `llvm.cttz.*`, `llvm.ctpop.*`, `llvm.bswap.*`.

#### 11.5 Memory operations
- `@memcpy`, `@memmove`, `@memset` → LLVM `llvm.memcpy.*`, `llvm.memmove.*`, `llvm.memset.*`.

#### 11.6 Raw pointer crossing (sealed conduit intrinsics)
- `@ptrToRef(T, ptr)`, `@refToPtr(ref)`, `@ptrOffset(ptr, n)`, `@ptrDiff(p1, p2)` → `bitcast` and `getelementptr` with `int64` difference. These are only allowed where `*T` is legal (under `@extern`).

#### 11.7 Other intrinsics
- `@bitcast(T, x)` → LLVM `bitcast`.
- `@typeId(T)` (new) – returns a unique integer ID for type `T` (used by `is` for fast comparisons).

### Phase 12: Module System & Visibility
- **Package declaration** – each `.luc` file belongs to a package; mangles symbols as `package_file_symbol`.
- **Visibility tiers**:
  - No keyword: file‑private → no external symbol.
  - `pub`: package‑visible → symbol exported with hidden visibility (ELF `protected`).
  - `export`: world‑visible → default visibility, placed in `.dynsym`.
- **Imports (`use`)** – resolved semantically; codegen inserts declarations of external functions/types.
- **Re‑exports (`export use`)** – symbol table aliasing; codegen emits weak aliases for re‑exported symbols.
- Circular imports prevented by sema.

### Phase 13: Enums
- **Enum declaration** – integer type (smallest fit: `i8`, `i16`, etc.). Each variant gets a constant integer value.
- **Variant access** `Direction.North` → constant integer.
- **Match on enum** – integer comparison; exhaustiveness checked by sema.
- **No payload** – enums are plain integers.

### Phase 14: (Removed – Union Types)
- Union types are **not** supported. Use `any` + `is` for heterogeneous containers.
- All union‑related parsing, semantic checks, and codegen have been removed.

### Phase 15: `any` & Boxing (detailed)
- **Box layout** (runtime):
  ```c
  struct any {
      void* data;                // pointer to boxed value (heap)
      TypeDescriptor* type;      // runtime type info
      uint64_t flags;            // 1 = owned, 2 = copy-on-write, etc.
  };
  ```
- **Boxing codegen**:
  - For small types (≤ 2 words): may be stored inline in the `data` pointer via NaN‑tagging (optimisation).
  - Otherwise: allocate heap memory, copy value (deep copy if owned), set type descriptor.
- **Unboxing** – `if val is T { … }`:
  - Compare `val.type` with `TypeDescriptor` of `T`.
  - If match, load `val.data` and cast to `T*`, then load the value.
  - Inside the branch, the variable is narrowed to `T`.
- **Any literals** – `let a any = 42` boxes immediately.
- **Performance considerations** – boxing is heap allocation; avoid in hot loops. Use static types when possible.

---

## 3. Integration Notes

### 3.1 Generic Impl Syntax
Correct: `impl Vec2<T> : Drawable { ... }`

### 3.2 Interaction of Conversions with Other Features
- Generics + conversions – generic `from` not allowed; monomorphizer handles generic structs in conversion signatures.
- Async + conversions – conversion functions can be `async` (return future of target).
- Parallel + conversions – no special handling.

### 3.3 Runtime Library (final list)
`luc_runtime` provides:
- Memory management (malloc/free for dynamic arrays, boxes, closures)
- Async scheduler and task pool
- Array operations (`.push`, `.pop`, etc.)
- String conversion helpers (`int_to_string`, etc.)
- Nullable unwrapping panic (`__luc_panic`)
- Type descriptors and dynamic type checking for `any`
- Raw pointer crossing helpers (via intrinsics, but runtime may provide fallbacks)

---

## 4. Implementation Order (Revised, Final)

1. Phase 0 – LLVM setup
2. Phase 1 – Basic expressions
3. Phase 1.5 – `any` type + boxing (minimal)
4. Phase 2 – Statements, `switch`, `match`
5. Phase 2.5 – `is` operator and type narrowing
6. Phase 3 – Functions, structs, references, deep copy
7. Phase 3.5 – Generics
8. Phase 4 – Impl methods, traits
9. Phase 5 – Conversions, `??`, implicit casts
10. Phase 6 – Arrays (all kinds)
11. Phase 7 – Advanced operators (`->`, `+>`, currying, closures, `.?`)
12. Phase 8 – Async/await
13. Phase 9 – Parallel execution
14. Phase 10 – JIT/AOT dual compilation
15. Phase 11 – FFI and all intrinsics (including `@typeId`)
16. Phase 12 – Module system and visibility
17. Phase 13 – Enums
18. (Phase 14 – skipped)
19. Phase 15 – Complete `any` boxing/unboxing and type descriptors

---

## 5. File Structure (Final)

```
src/codegen/
├── CodeGen.cpp
├── CodeGenDecl.cpp
├── CodeGenExpr.cpp
├── CodeGenStmt.cpp
├── ValueEnv.hpp
├── GenericSpecialization.cpp
├── GenericCache.hpp
├── TypeSubstitution.cpp
├── ConversionBuiltin.cpp
├── ConversionUser.cpp
├── CoercionPass.cpp
├── CoroutineTransform.cpp
├── AsyncRuntime.cpp
├── ParallelTransform.cpp
├── CompilationDriver.cpp
├── AOTCompiler.cpp
├── JITEngine.cpp
├── BytecodeFormat.hpp
├── ModuleSystem.cpp
├── EnumCodeGen.cpp
├── AnyBoxing.cpp               # boxing/unboxing, runtime type info
├── IsOperator.cpp              # is lowering, type narrowing
└── RawPointerIntrinsics.cpp

src/runtime/
├── luc_runtime.h
├── async_scheduler.cpp
├── task_pool.cpp
├── array_ops.cpp
├── conversion_helpers.cpp
├── memory.cpp
├── any_box.c                   # any box allocation, type descriptor tables
├── type_descriptor.c
├── nullable.cpp
└── enum_helpers.cpp            # (minimal)
```

---

## 6. Milestones & Success Criteria

- **M1 (Week 2):** Expressions, control flow, `switch`, basic `match`.
- **M2 (Week 3):** `any` and `is` with primitive boxing.
- **M3 (Week 4):** Functions, structs, nullable, `??`.
- **M4 (Week 5):** Generics + traits.
- **M5 (Week 6):** User‑defined conversions, implicit casting.
- **M6 (Week 7):** Enums, arrays (all kinds).
- **M7 (Week 8):** Advanced: currying, closures, pipelines.
- **M8 (Week 9):** Async/await.
- **M9 (Week 10):** Parallel execution.
- **M10 (Week 11):** Full FFI, all intrinsics, `any` complete.
- **M11 (Week 12):** Module system, JIT/AOT end‑to‑end.

**Final test:** The entire Luc test suite (including `any_test.luc`) compiles and runs correctly.

---

This plan now covers **every** feature of the Luc language as defined in `LUC_GRAMMAR.md`, with union types replaced by `any` + `is` as requested. It is ready for implementation.
```