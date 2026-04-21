# Luc Lexer Phase

The Luc Lexer (Scanner) is the first stage of the compilation pipeline. Its responsibility is to transform a raw stream of source characters into a sequence of meaningful tokens used by the Parser.

## Responsibility

The Lexer performs the following tasks:
- **Scanning:** Reads the source file character by character.
- **Filtering:** Discards whitespace and non-documentation comments.
- **Classification:** Groups characters into lexemes and assigns them a `TokenType`.
- **Metadata Tagging:** Records the line and column number for every token to support accurate error reporting.

## Token Structure

Defined in [Tokens.hpp](../../src/Tokens.hpp), a token is a simple data structure:

```cpp
struct Token {
    TokenType type;
    std::string value; // The raw lexeme (e.g., "123", "myVar")
    int line;
    int column;
};
```

## Tokenization Process

The Lexer uses a standard "peek and advance" mechanism. The core logic resides in `Lexer::getNextToken()`.

### 1. Whitespace and Comments

The Lexer skips standard whitespace (` `, `\t`, `\r`, `\n`). However, Luc handles comments with specific granularity:

| Comment Style | Implementation | Lexer Action |
| :--- | :--- | :--- |
| `/- ... -/` | Block Comment | Discarded during `skipWhitespace()`. |
| `-- ...` | Line Comment | Emitted as `LINE_COMMENT`. Used to harvest doc comments. |
| `/-- ... --/` | Doc Comment | Emitted as `DOC_COMMENT`. Attached to subsequent declarations by the Parser. |

### 2. Identifiers and Keywords

When the Lexer encounters an alphabetic character or underscore, it consumes all subsequent alphanumeric characters. 

- **Keyword Matching:** The resulting string is checked against a hash map of reserved keywords (e.g., `package`, `struct`, `impl`).
- **Raw Strings:** If the identifier is exactly `r` and is followed immediately by a double quote (`r"`), the Lexer enters `readRawString()` mode.
- **Wildcard:** The standalone underscore `_` is emitted as `TokenType::WILDCARD`.

### 3. Literals

The Lexer implements specialized readers for Luc's rich literal support:

#### Numbers (`readNumber`)
Supports multiple bases and formats:
- **Hexadecimal:** `0x` followed by `[0-9a-fA-F_]`.
- **Binary:** `0b` followed by `[01_]`.
- **Decimal/Float:** Supports underscores as digit separators (e.g., `1_000_000`) and scientific notation (e.g., `1.5e-10`).

#### Strings (`readString` and `readRawString`)
- **Standard Strings:** Enclosed in `"`. Supports escape sequences like `\n`, `\t`, `\xHH`, `\uXXXX`, and `\UXXXXXXXX`.
- **Raw Strings:** Prefixed with `r` (e.g., `r"C:\Users"`). No escape processing occurs; backslashes are stored literally.

#### Characters (`readChar`)
- Enclosed in `'`. Supports standard character escapes.

### 4. Operators and Delimiters

Luc has many multi-character operators. The Lexer disambiguates these using a lookahead (`match` method):

- `.` vs `.?` (Nullable Chain) vs `..` (Range) vs `...` (Variadic)
- `?` vs `??` (Nil Fallback)
- `=` vs `==` (Equality)
- `-` vs `->` (Pipeline) vs `-=` (Decrement)
- `+` vs `+>` (Composition) vs `+=` (Increment)

## Mapping to Grammar

The Lexer's output directly corresponds to the terminals defined in [LUC_GRAMMAR.md](../LUC_GRAMMAR.md).

### Top-Level Keywords
Reflects the `top_level_decl` section of the grammar:
- `package`, `use`, `as`, `impl`, `type`, `struct`, `enum`, `trait`, `from`.

### Types
Reflects the `primitive_type` and `ptr_type` sections:
- `bool`, `int`, `uint8`, `float`, `string`, `any`, etc.
- `&` (Reference) and `*` (Raw Pointer - `@extern` only).

### Modifiers
Reflects visibility and performance modifiers:
- `pub`, `export`, `async`, `await`, `parallel`.

## Error Handling

If the Lexer encounters a character it does not recognize (and is not inside a string or comment), it emits a `TokenType::UNKNOWN`. This allows the Parser to provide a high-quality error message containing the exact character and its location rather than failing silently.
