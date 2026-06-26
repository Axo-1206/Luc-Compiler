// =============================================================================
// parseDeclaration() – Parses a declaration in any context
// =============================================================================

/**
 * @brief Parse a declaration in the current context.
 * 
 * This function dispatches to the appropriate declaration parser based on
 * the context (top-level or local). It handles:
 * - Top-level declarations: structs, enums, traits, functions, variables, use
 * - Local declarations: variables, functions, structs, enums, traits
 * 
 * ## Context Differences
 * 
 * **Top-Level Context** (`ctx == TopLevel`):
 * - Use declarations are allowed
 * - Attributes like `@[export]` are allowed
 * 
 * **Local Context** (`ctx == Local`):
 * - Use declarations are NOT allowed
 * - `@[export]` is rejected
 * - Only declarations that make sense in a block are allowed
 * 
 * ## Error Handling
 * 
 * If a declaration fails to parse, the function returns nullptr and the
 * caller is responsible for error recovery. The parser state's `hasErrors`
 * flag will be set.
 * 
 * @param state Parser state with token stream and context.
 * @param ctx   The declaration context (TopLevel or Local).
 * @return DeclAST* The parsed declaration node, or nullptr on error.
 */
DeclAST* parseDeclaration(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseTopLevelDecl() – Parses a top-level declaration
// =============================================================================

/**
 * @brief Parse a top-level declaration.
 * 
 * This function attempts to parse any valid top-level declaration:
 * - `use` declarations
 * - `struct` definitions
 * - `enum` definitions
 * - `trait` definitions
 * - `let` or `const` variable declarations
 * - Function declarations
 * 
 * ## Order of Operations
 * 
 * 1. Harvest doc comments (for documentation generation)
 * 2. Parse attributes (like `@[export]`, `@[deprecated]`)
 * 3. Determine the declaration type from the current token
 * 4. Dispatch to the appropriate parser
 * 5. Attach doc comments and attributes to the parsed node
 * 
 * ## Error Handling
 * 
 * If the current token doesn't match any declaration type, a generic error
 * is reported and the parser synchronizes to the next safe token.
 * 
 * @param state Parser state with token stream.
 * @return DeclAST* The parsed declaration node, or nullptr on error.
 */
DeclAST* parseTopLevelDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseUseDecl() – Parses a 'use' declaration
// =============================================================================

/**
 * @brief Parse a `use` declaration.
 * 
 * A `use` declaration imports symbols from another module:
 * 
 * ```lucid
 * use std.io
 * use std.math as math
 * use graphics.gl as gl
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * use_decl := 'use' IDENTIFIER { '.' IDENTIFIER } [ 'as' IDENTIFIER ]
 * ```
 * 
 * ## Module Resolution
 * 
 * The path segments (e.g., `std.io`) are resolved relative to the package
 * root. The semantic pass will actually resolve and parse the imported
 * module.
 * 
 * ## Error Handling
 * 
 * - Missing path: `E1102` ("Expected module path after keyword 'use'")
 * - Missing alias after `as`: A generic error is reported
 * 
 * @param state Parser state with token stream.
 * @return UseDeclAST* The parsed use declaration, or nullptr on error.
 */
UseDeclAST* parseUseDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseVarDecl() – Parses a variable declaration
// =============================================================================

/**
 * @brief Parse a variable declaration.
 * 
 * Variable declarations create named bindings with an explicit type:
 * 
 * ```lucid
 * let x int = 42
 * const pi float = 3.14159
 * let name string? = nil
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * var_decl := ('let' | 'const') IDENTIFIER type [ '=' expr ]
 * ```
 * 
 * ## Rules
 * 
 * - Type is always required (no type inference)
 * - `const` requires an initializer
 * - `let` may omit the initializer
 * 
 * ## Error Handling
 * 
 * - Missing identifier: Generic error reported
 * - Missing type: Generic error reported
 * - `const` without initializer: `E2030` ("'const' missing initialiser")
 * 
 * @param state Parser state with token stream.
 * @return VarDeclAST* The parsed variable declaration, or nullptr on error.
 */
VarDeclAST* parseVarDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseFuncDecl() – Parses a function declaration
// =============================================================================

/**
 * @brief Parse a function declaration.
 * 
 * Function declarations define callable entities with parameters and a body:
 * 
 * ```lucid
 * const add (a int)(b int) -> int = { return a + b }
 * const makeAdder (base int) -> (n int) -> int = { ... }
 * const sum (nums ...int) -> int = { ... }
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * func_decl := ('let' | 'const') IDENTIFIER [ generic_params ]
 *              param_group { param_group }
 *              [ '->' return_type ]
 *              '=' func_body
 * ```
 * 
 * ## Currying and Form 2 `()()` Shorthand
 * 
 * The parser desugars Form 2 `()()` shorthand into nested Form 1 functions
 * before building the AST. The `funcType` captures the full curried structure:
 * 
 * ```
 * const clamp (lo int)(hi int)(v int) -> int
 * → FuncTypeAST: (lo int) -> (hi int) -> (v int) -> int
 * ```
 * 
 * ## Error Handling
 * 
 * - Missing identifier: Generic error reported
 * - Invalid function type: Generic error reported
 * - Missing body: Generic error reported
 * 
 * @param state Parser state with token stream.
 * @return FuncDeclAST* The parsed function declaration, or nullptr on error.
 */
FuncDeclAST* parseFuncDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseStructDecl() – Parses a struct declaration
// =============================================================================

/**
 * @brief Parse a struct declaration.
 * 
 * Struct declarations define composite data types with named fields:
 * 
 * ```lucid
 * struct Point { x float = 0.0, y float = 0.0 }
 * struct Node<T> { value T, next ptr<Node<T>>? }
 * struct Entity : Vector2, Named { name string, x float, y float }
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * struct_decl := 'struct' IDENTIFIER [ generic_params ]
 *                [ ':' trait_ref { ',' trait_ref } ]
 *                '{' { struct_field } '}'
 * ```
 * 
 * ## Features
 * 
 * - **Generic parameters**: `<T>`, `<T : Trait>`
 * - **Trait implementations**: `: Vector2, Named`
 * - **Const fields**: `step const int`
 * - **Default values**: `x float = 0.0`
 * 
 * ## Error Handling
 * 
 * - Missing name: Generic error reported
 * - Missing field type: Generic error reported
 * - Const field without default: Reported in `parseFieldDecl`
 * 
 * @param state Parser state with token stream.
 * @return StructDeclAST* The parsed struct declaration, or nullptr on error.
 */
StructDeclAST* parseStructDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseEnumDecl() – Parses an enum declaration
// =============================================================================

/**
 * @brief Parse an enum declaration.
 * 
 * Enum declarations define a set of named integer constants:
 * 
 * ```lucid
 * enum Direction { North = 0, East = 1, South = 2, West = 3 }
 * enum Status : int32 { Ok = 200, NotFound = 404, Error = 500 }
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * enum_decl := 'enum' IDENTIFIER [ ':' int_type ] '{' { enum_variant } '}'
 * enum_variant := IDENTIFIER '=' INT_LIT
 * ```
 * 
 * ## Rules
 * 
 * - Values are required (no auto-increment)
 * - Values must be integer literals
 * - Values must be unique within the enum
 * 
 * ## Error Handling
 * 
 * - Missing name: Generic error reported
 * - Missing value: `E1108` ("Invalid integer literal '%s' for enum variant")
 * - Backing type must be an integer type
 * 
 * @param state Parser state with token stream.
 * @return EnumDeclAST* The parsed enum declaration, or nullptr on error.
 */
EnumDeclAST* parseEnumDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseTraitDecl() – Parses a trait declaration
// =============================================================================

/**
 * @brief Parse a trait declaration.
 * 
 * Trait declarations define field contracts that structs can implement:
 * 
 * ```lucid
 * trait Vector2 { x float, y float }
 * trait Named { name string }
 * trait Container<T> { value T, count int }
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * trait_decl := 'trait' IDENTIFIER [ generic_params ] '{' { trait_field } '}'
 * trait_field := [ 'const' ] IDENTIFIER type
 * ```
 * 
 * ## Rules
 * 
 * - Trait fields are name and type only (no default values)
 * - Trait fields can be marked `const`
 * - Trait fields cannot be nullable or fallible
 * - Traits can be generic
 * 
 * ## Error Handling
 * 
 * - Missing name: Generic error reported
 * - Missing field type: Generic error reported
 * - Nullable/fallible trait field: Reported in `parseTraitField`
 * 
 * @param state Parser state with token stream.
 * @return TraitDeclAST* The parsed trait declaration, or nullptr on error.
 */
TraitDeclAST* parseTraitDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseFieldDecl() – Parses a struct field
// =============================================================================

/**
 * @brief Parse a struct field declaration.
 * 
 * Struct fields define the data members of a struct:
 * 
 * ```lucid
 * x float = 0.0
 * step const int
 * items [*]string
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * struct_field := [ 'const' ] IDENTIFIER type [ '=' expr ]
 * ```
 * 
 * ## Rules
 * 
 * - Name then type (Go-style)
 * - Const fields cannot be reassigned after construction
 * - Const fields must have a default value
 * - Const fields cannot be nullable or fallible
 * 
 * ## Error Handling
 * 
 * - Missing name: Generic error reported
 * - Missing type: Generic error reported
 * - Const without default: Generic error reported
 * 
 * @param state Parser state with token stream.
 * @return FieldDeclAST* The parsed field declaration, or nullptr on error.
 */
FieldDeclAST* parseFieldDecl(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseEnumVariant() – Parses an enum variant
// =============================================================================

/**
 * @brief Parse an enum variant.
 * 
 * Enum variants define named integer constants:
 * 
 * ```lucid
 * North = 0
 * East = 1
 * South = 2
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * enum_variant := IDENTIFIER '=' INT_LIT
 * ```
 * 
 * ## Rules
 * 
 * - Value is required (no auto-increment)
 * - Value must be an integer literal (decimal, hex, or binary)
 * - Values must be unique within the enum
 * 
 * @param state Parser state with token stream.
 * @return EnumVariantAST* The parsed enum variant, or nullptr on error.
 */
EnumVariantAST* parseEnumVariant(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseTraitField() – Parses a trait field
// =============================================================================

/**
 * @brief Parse a trait field declaration.
 * 
 * Trait fields define the requirements for structs implementing the trait:
 * 
 * ```lucid
 * x float
 * name string
 * const id int
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * trait_field := [ 'const' ] IDENTIFIER type
 * ```
 * 
 * ## Rules
 * 
 * - Name then type (Go-style)
 * - No default values allowed
 * - Can be marked `const`
 * - Cannot be nullable or fallible
 * 
 * ## Error Handling
 * 
 * - Missing name: Generic error reported
 * - Missing type: Generic error reported
 * - Nullable/fallible field: Reported with error
 * 
 * @param state Parser state with token stream.
 * @return TraitFieldPtr The parsed trait field, or nullptr on error.
 */
TraitFieldPtr parseTraitField(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

// =============================================================================
// parseTraitRef() – Parses a trait reference
// =============================================================================

/**
 * @brief Parse a trait reference.
 * 
 * Trait references appear in struct declarations and generic constraints:
 * 
 * ```lucid
 * struct Entity : Vector2, Named { ... }
 * const magnitude<T : Vector2> (v T) -> float = { ... }
 * ```
 * 
 * ## Grammar
 * 
 * ```
 * trait_ref := IDENTIFIER [ '<' type { ',' type } '>' ]
 * ```
 * 
 * ## Examples
 * 
 * - `Vector2` → name = "Vector2", genericArgs = {}
 * - `Container<int>` → name = "Container", genericArgs = {int}
 * - `Map<string, Vec2>` → name = "Map", genericArgs = {string, Vec2}
 * 
 * @param state Parser state with token stream.
 * @return TraitRefAST* The parsed trait reference, or nullptr on error.
 */
TraitRefAST* parseTraitRef(ParserState& state) {
    // The implementation is in ParserDecl.cpp
    // This declaration is for documentation purposes only.
    return nullptr;
}

