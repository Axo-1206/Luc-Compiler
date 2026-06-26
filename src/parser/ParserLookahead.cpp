#include "parser/Parser.hpp"
#include "core/diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugMacros.hpp"
#include "debug/DebugUtils.hpp"

namespace parser {
    
    /**
    * @brief Check if a token is a declaration start.
    * 
    * This function checks if the current token could start a declaration.
    * 
    * @param state The parser state.
    * @return true if the current token starts a declaration.
    * 
    * ## Declaration Start Tokens
    * 
    * - `use`, `struct`, `enum`, `trait`, `let`, `const`
    * - `IDENTIFIER` (could be a function declaration)
    */
    bool isStartOfDeclaration(ParserState& state) {
        TokenType type = state.stream.peekType();
        switch (type) {
            case TokenType::USE:
            case TokenType::STRUCT:
            case TokenType::ENUM:
            case TokenType::TRAIT:
            case TokenType::LET:
            case TokenType::CONST:
                return true;
            case TokenType::IDENTIFIER:
                // Could be a function declaration
                return looksLikeFuncDecl(state);
            default:
                return false;
        }
    }

    /**
    * @brief Check if a token is a statement start.
    * 
    * This function checks if the current token could start a statement.
    * 
    * @param state The parser state.
    * @return true if the current token starts a statement.
    * 
    * ## Statement Start Tokens
    * 
    * - `if`, `switch`, `for`, `while`, `do`, `return`, `break`, `continue`
    * - `{` (block)
    * - `let`, `const` (declaration statements)
    * - `IDENTIFIER` (could be an expression statement)
    * - Literals (expression statements)
    */
    bool isStartOfStatement(const ParserState& state) {
        TokenType type = state.stream.peekType();
        switch (type) {
            case TokenType::IF:
            case TokenType::SWITCH:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::DO:
            case TokenType::RETURN:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::LBRACE:
            case TokenType::LET:
            case TokenType::CONST:
                return true;
            case TokenType::IDENTIFIER:
            case TokenType::INT_LITERAL:
            case TokenType::FLOAT_LITERAL:
            case TokenType::STRING_LITERAL:
            case TokenType::CHAR_LITERAL:
            case TokenType::HEX_LITERAL:
            case TokenType::BINARY_LITERAL:
            case TokenType::TRUE:
            case TokenType::FALSE:
            case TokenType::NIL:
            case TokenType::ERR:
            case TokenType::UNDERSCORE:
                // Expression statement or variable declaration
                return true;
            default:
                return false;
        }
    }

    /**
    * @brief Check if a token is a type start.
    * 
    * This function checks if the current token could start a type annotation.
    * 
    * @param state The parser state.
    * @return true if the current token starts a type.
    * 
    * ## Type Start Tokens
    * 
    * - Primitive types: `bool`, `int`, `float`, `string`, etc.
    * - `IDENTIFIER` (named types)
    * - `[` (array types)
    * - `&` (reference types)
    * - `(` (function types)
    */
    bool isStartOfType(const ParserState& state) {
        TokenType type = state.stream.peekType();
        if (state.stream.isPrimitiveTypeToken(type)) return true;
        switch (type) {
            case TokenType::IDENTIFIER:
            case TokenType::LBRACKET:
            case TokenType::AMPERSAND:
            case TokenType::LPAREN:
                return true;
            default:
                return false;
        }
    }

} // namespace parser

