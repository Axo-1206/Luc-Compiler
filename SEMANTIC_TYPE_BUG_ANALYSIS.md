# Semantic Type Mismatch Bug Analysis

## Problem Statement
Function types are always mismatched when comparing:
- Function declarations with signatures like `(a int) int`
- Variable assignments using those same function types

## Root Cause
The bug is **NOT in the type comparison logic** (TypeChecker.cpp). Instead, it's a **parser ambiguity** that causes incorrect AST construction.

### The Core Issue

The test file contains:
```luc
const core_add (a int) int = {
    return a 
}

export const main () int = {
    let myAdd (a int) int = core_add   // ← ERROR LINE
    ...
}
```

### What Should Happen
1. `let myAdd (a int) int = core_add` should be parsed as:
   - **VarDeclAST** (variable declaration)
   - Type annotation: `FuncTypeAST { params: [int], returnType: int }`
   - Initializer: identifier `core_add` (which has type `FuncTypeAST`)
   - These types should match via `TypeChecker::isAssignable`

### What Actually Happens
1. `looksLikeFuncDecl()` in Parser.cpp:410 checks for `(` after the name
2. It finds `(` from `let myAdd (**a int**) int = ...`
3. It **wrongly decides this is a FUNCTION declaration**, not a variable!
4. Parser treats it as: `const myFunc (a int) int = core_add`
5. Semantic checker tries to validate `core_add` as a function body
6. Fails: "return type mismatch" because `core_add` (an identifier expression) is not a valid function body

## The Parser Bug Code Location

**File**: `src/parser/Parser.cpp:410-440`

```cpp
bool Parser::looksLikeFuncDecl() const {
    std::size_t i = pos_;

    // Skip the name IDENTIFIER
    if (i < tokens_.size() && tokens_[i].type == TokenType::IDENTIFIER) {
        ++i;
    }

    // Skip generic params if present: < ... >
    // ... (code omitted)

    // After optional generics, a '(' means function declaration.
    // ⚠️ BUG: This is WRONG! It can't distinguish between:
    //    - let func (a int) int = { body }  ← function declaration
    //    - let var  (a int) int = expr      ← variable with function type  
    return (i < tokens_.size() && tokens_[i].type == TokenType::LPAREN);
}
```

## Why TypeChecker.cpp is Not the Problem

The TypeChecker's `isEqual()` function (lines 57-66) is **correct**:

```cpp
if (a->isa<FuncTypeAST>()) {
    auto* fa = a->as<FuncTypeAST>();
    auto* fb = b->as<FuncTypeAST>();
    if (fa->isNullable != fb->isNullable) return false;
    if (fa->params.size() != fb->params.size()) return false;
    for (size_t i = 0; i < fa->params.size(); ++i) {
        if (!isEqual(fa->params[i].get(), fb->params[i].get())) return false;
    }
    return isEqual(fa->returnType.get(), fb->returnType.get());
}
```

If the types actually reached TypeChecker, this code would correctly compare them. The issue is that the `let myAdd` declaration **never reaches the type checker's comparison** because it's misparsed as a function declaration, not a variable declaration.

## The Fix Strategy

The parser needs to distinguish between:
1. **Function Declaration**: `let func (params) ret = { stmts }`
2. **Variable with Function Type**: `let var (params) ret = expr`

Key insight: After the type annotation, what follows?
- Function decl: `=` followed by a **block body** `{ ... }` or **lambda** `|...` or **async**
- Variable decl: `=` followed by an **expression**

The parser could disambiguate by looking **further ahead** (past the type annotation) to see what follows the `=` sign. Only if there's a `{`, `|`, or `async` should it be treated as a function declaration.

## Additional Issue: Empty Parameter Groups

Another issue in SemanticCollector.cpp (lines 85-94): When copying NamedTypeAST in function signatures during Phase 1, the code **loses generic arguments**:

```cpp
else if (p->type->kind == ASTKind::NamedType) {
    // ⚠️ BUG: Only copies the name, loses genericArgs!
    ft->params.push_back(std::make_unique<NamedTypeAST>(
        static_cast<NamedTypeAST*>(p->type.get())->name));
}
```

If a parameter was declared as `Vector<int>`, this becomes just `Vector`, losing the generic argument information.

## Next Steps

1. **Primary Fix**: Update parser lookahead to distinguish function declarations from variable declarations with function types
2. **Secondary Fix**: Preserve generic arguments when copying NamedTypeAST in SemanticCollector
3. **Test**: Verify that `let myAdd (a int) int = core_add` now correctly parses as a VarDeclAST with proper type matching
