#include "Lexer.hpp"
#include "Tokens.hpp"
#include "debug/DebugMacros.hpp"
#include <cctype>
#include <unordered_map>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Lexer State Structure Definition
// Holds all state information for the lexical analysis process.
// An instance is created per source file and is opaque to external code.
// ─────────────────────────────────────────────────────────────────────────────
struct LexerState {
    // ─────────────────────────────────────────────────────────────────────────
    // Input Source
    // ─────────────────────────────────────────────────────────────────────────
    
    /// The complete source code string being tokenized.
    /// This is the raw input buffer that the lexer scans character by character.
    std::string src;
    
    /// Interned file path (e.g., "src/main.luc") used exclusively for diagnostic
    /// error messages. The lexer never reads from this; it only passes it to
    /// diagnostic::error() when reporting invalid characters or other lexical issues.
    InternedString fileName;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Scanning Position
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Current scan position (0-based index) within src.
    /// Points to the next character to be read by advance().
    /// Incremented each time a character is consumed.
    size_t pos;
    
    /// Current line number (1-based). Incremented when a newline ('\n') is
    /// encountered. Used for token location tracking and error reporting.
    int line;
    
    /// Current column number (1-based) within the current line.
    /// Resets to 1 after each newline. Incremented with each character consumed.
    int column;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Keyword Lookup Table
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Maps reserved keyword strings (e.g., "if", "while", "struct") to their
    /// corresponding TokenType enum values. Initialized in initKeywords().
    /// Enables O(1) lookup when distinguishing identifiers from keywords.
    std::unordered_map<std::string, TokenType> keywords;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Construction & Initialization
    // ─────────────────────────────────────────────────────────────────────────
    
    /// Constructs a new lexer state for the given source code and file name.
    /// Initializes position to the start (pos=0, line=1, column=1) and builds
    /// the keyword lookup table.
    /// 
    /// @param source The complete source code string to tokenize.
    /// @param fname  Interned file path for diagnostic messages.
    LexerState(const std::string& source, InternedString fname)
        : src(source), fileName(fname), pos(0), line(1), column(1) {
        initKeywords();
    }
    
private:
    /// Populates the keywords map with all reserved words in the language.
    /// Called once during construction. Includes:
    ///   - Modifiers (pub, export)
    ///   - Type keywords (int, string, bool, etc.)
    ///   - Control flow (if, else, while, for, etc.)
    ///   - Logical operators (and, or, not)
    ///   - Error handling (resolve, ok, err)
    void initKeywords();
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper function declarations
// ─────────────────────────────────────────────────────────────────────────────
static bool isAtEnd(const LexerState* state);
static char peek(const LexerState* state);
static char peekNext(const LexerState* state);
static char peekNextOffset(const LexerState* state, int offset);
static char advance(LexerState* state);
static bool match(LexerState* state, char expected);
static Token makeToken(LexerState* state, TokenType type, const std::string& value);
static void skipWhitespace(LexerState* state);
static Token readNumber(LexerState* state, char first);
static Token readString(LexerState* state);
static Token readRawString(LexerState* state, int hashCount);
static Token readChar(LexerState* state);
static Token readLineComment(LexerState* state);
static Token readDocComment(LexerState* state);
static Token getNextToken(LexerState* state);

// ─────────────────────────────────────────────────────────────────────────────
// Keyword Initialization
// ─────────────────────────────────────────────────────────────────────────────
void LexerState::initKeywords() {
    // ── Modifiers ──────────────────────────────────────────────────────────────
    keywords["pub"]     = TokenType::PUB;
    keywords["export"]  = TokenType::EXPORT;

    // ── Top Level ──────────────────────────────────────────────────────────────
    keywords["package"] = TokenType::PACKAGE;
    keywords["use"]     = TokenType::USE;
    keywords["as"]      = TokenType::AS;
    keywords["impl"]    = TokenType::IMPL;
    keywords["type"]    = TokenType::TYPE;
    keywords["struct"]  = TokenType::STRUCT;
    keywords["enum"]    = TokenType::ENUM;
    keywords["trait"]   = TokenType::TRAIT;
    keywords["from"]    = TokenType::FROM;

    // ── Declarations ───────────────────────────────────────────────────────────
    keywords["let"]     = TokenType::LET;
    keywords["const"]   = TokenType::CONST;

    // ── Concurrency ────────────────────────────────────────────────────────────
    keywords["await"]   = TokenType::AWAIT;

    // ── Primary Types ──────────────────────────────────────────────────────────
    keywords["bool"]    = TokenType::TYPE_BOOL;

    // Signed integers
    keywords["byte"]    = TokenType::TYPE_BYTE;
    keywords["short"]   = TokenType::TYPE_SHORT;
    keywords["int"]     = TokenType::TYPE_INT;
    keywords["long"]    = TokenType::TYPE_LONG;

    // Unsigned integers
    keywords["ubyte"]   = TokenType::TYPE_UBYTE;
    keywords["ushort"]  = TokenType::TYPE_USHORT;
    keywords["uint"]    = TokenType::TYPE_UINT;
    keywords["ulong"]   = TokenType::TYPE_ULONG;

    // Fixed-width (critical for Vulkan struct layouts)
    keywords["int8"]    = TokenType::TYPE_INT8;
    keywords["int16"]   = TokenType::TYPE_INT16;
    keywords["int32"]   = TokenType::TYPE_INT32;
    keywords["int64"]   = TokenType::TYPE_INT64;
    keywords["uint8"]   = TokenType::TYPE_UINT8;
    keywords["uint16"]  = TokenType::TYPE_UINT16;
    keywords["uint32"]  = TokenType::TYPE_UINT32;
    keywords["uint64"]  = TokenType::TYPE_UINT64;

    // Floating point
    keywords["float"]   = TokenType::TYPE_FLOAT;
    keywords["double"]  = TokenType::TYPE_DOUBLE;
    keywords["decimal"] = TokenType::TYPE_DECIMAL;

    // Text
    keywords["string"]  = TokenType::TYPE_STRING;
    keywords["char"]    = TokenType::TYPE_CHAR;

    // Special
    keywords["any"] = TokenType::TYPE_ANY;
    keywords["nil"] = TokenType::NIL;

    // ── Control Flow ───────────────────────────────────────────────────────────
    keywords["if"]      = TokenType::IF;
    keywords["else"]    = TokenType::ELSE;
    keywords["match"]   = TokenType::MATCH;
    keywords["switch"]  = TokenType::SWITCH;
    keywords["case"]    = TokenType::CASE;
    keywords["default"] = TokenType::DEFAULT;
    keywords["is"]      = TokenType::IS;
    keywords["while"]   = TokenType::WHILE;
    keywords["for"]     = TokenType::FOR;
    keywords["in"]      = TokenType::IN;
    keywords["do"]      = TokenType::DO;
    keywords["return"]  = TokenType::RETURN;
    keywords["break"]   = TokenType::BREAK;
    keywords["continue"] = TokenType::CONTINUE;

    // ── Logical ────────────────────────────────────────────────────────────────
    keywords["and"]     = TokenType::AND;
    keywords["or"]      = TokenType::OR;
    keywords["not"]     = TokenType::NOT;
    keywords["true"]    = TokenType::TRUE;
    keywords["false"]   = TokenType::FALSE;

    // ─── Error Handling ───────────────────────────────────────────────────────
    keywords["resolve"] = TokenType::RESOLVE;
    keywords["ok"]      = TokenType::OK;
    keywords["err"]     = TokenType::ERR;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle Implementation
// ─────────────────────────────────────────────────────────────────────────────
LexerState* createLexer(const std::string& source, InternedString fileName) {
    LUC_LOG_LEXER("Lexer constructed, source size: " << source.size() << " bytes");
    return new LexerState(source, fileName);
}

void destroyLexer(LexerState* state) {
    delete state;
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors Implementation
// ─────────────────────────────────────────────────────────────────────────────
int getLexerLine(const LexerState* state) {
    return state ? state->line : 0;
}

int getLexerColumn(const LexerState* state) {
    return state ? state->column : 0;
}

size_t getLexerPosition(const LexerState* state) {
    return state ? state->pos : 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper Implementations
// ─────────────────────────────────────────────────────────────────────────────
static Token makeToken(LexerState* state, TokenType type, const std::string& value) {
    LUC_LOG_LEXER_EXTREME("makeToken: type=" << static_cast<int>(type) << ", value='" << value << "', line=" << state->line << ", col=" << state->column);
    return {type, value, state->line, state->column};
}

static char peek(const LexerState* state) { 
    char c = isAtEnd(state) ? '\0' : state->src[state->pos];
    LUC_LOG_LEXER_EXTREME("peek: '" << (c == '\n' ? '\\' : c) << "' at pos=" << state->pos);
    return c;
}

static char peekNext(const LexerState* state) { 
    char c = (state->pos + 1 >= state->src.size()) ? '\0' : state->src[state->pos + 1];
    LUC_LOG_LEXER_EXTREME("peekNext: '" << (c == '\n' ? '\\' : c) << "'");
    return c;
}

static char peekNextOffset(const LexerState* state, int offset) {
    size_t idx = state->pos + offset;
    return (idx < state->src.size()) ? state->src[idx] : '\0';
}

static char advance(LexerState* state) {
    char c = state->src[state->pos++];
    state->column++;
    LUC_LOG_LEXER_EXTREME("advance: consumed '" << (c == '\n' ? '\\' : c) << "', new pos=" << state->pos);
    return c;
}

static bool isAtEnd(const LexerState* state) { 
    bool end = state->pos >= state->src.size();
    LUC_LOG_LEXER_EXTREME("isAtEnd: " << (end ? "true" : "false"));
    return end;
}

static bool match(LexerState* state, char expected) {
    if (isAtEnd(state) || state->src[state->pos] != expected) {
        LUC_LOG_LEXER_EXTREME("match('" << expected << "'): false");
        return false;
    }
    state->pos++;
    state->column++;
    LUC_LOG_LEXER_EXTREME("match('" << expected << "'): true");
    return true;
}

// Returns number of bytes in the UTF-8 sequence starting at 'c' (1-4), or 0 if invalid.
static int utf8SequenceLength(unsigned char c) {
    if (c < 0x80) return 1;               // ASCII
    if ((c & 0xE0) == 0xC0) return 2;     // 2-byte: 110xxxxx
    if ((c & 0xF0) == 0xE0) return 3;     // 3-byte: 1110xxxx
    if ((c & 0xF8) == 0xF0) return 4;     // 4-byte: 11110xxx
    return 0;                             // Invalid UTF-8 start
}

// ─────────────────────────────────────────────────────────────────────────────
// Whitespace & Comments
// ─────────────────────────────────────────────────────────────────────────────
static void skipWhitespace(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("skipWhitespace: pos=" << state->pos << ", line=" << state->line);
    
    while (!isAtEnd(state)) {
        char c = peek(state);

        if (c == ' ' || c == '\r' || c == '\t') {
            LUC_LOG_LEXER_EXTREME("skipWhitespace: skipping whitespace char '" << c << "'");
            advance(state);
        } else if (c == '\n') {
            LUC_LOG_LEXER_VERBOSE("skipWhitespace: newline, line=" << state->line << "->" << state->line + 1);
            state->line++;
            state->column = 1;
            advance(state);
        }
        // Single-line comment: --
        else if (c == '-' && peekNext(state) == '-') {
            LUC_LOG_LEXER_VERBOSE("skipWhitespace: found line comment start '--', breaking");
            break;
        }
        // Block comment: /- ... -/
        // Doc comment:   /-- ... --/
        else if (c == '/' && peekNext(state) == '-') {
            bool isDoc = (state->pos + 1 < state->src.size() && state->src[state->pos + 1] == '-');
            if (isDoc) {
                LUC_LOG_LEXER_VERBOSE("skipWhitespace: found doc comment '/--', breaking");
                break;
            }

            // Plain block comment /- ... -/
            LUC_LOG_LEXER_VERBOSE("skipWhitespace: skipping block comment");
            advance(state); // consume '/'
            advance(state); // consume '-'
            while (!isAtEnd(state) && !(peek(state) == '-' && peekNext(state) == '/')) {
                if (peek(state) == '\n') {
                    state->line++;
                    state->column = 1;
                }
                advance(state);
            }
            if (!isAtEnd(state)) {
                advance(state);
                advance(state);
            }
        } else {
            break;
        }
    }
    LUC_LOG_LEXER_VERBOSE("skipWhitespace: done, pos=" << state->pos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Literal readers
// ─────────────────────────────────────────────────────────────────────────────
static Token readNumber(LexerState* state, char first) {
    LUC_LOG_LEXER_VERBOSE("readNumber: first char='" << first << "'");
    std::string num(1, first);

    // Hex: 0xFF
    if (first == '0' && (peek(state) == 'x' || peek(state) == 'X')) {
        LUC_LOG_LEXER_VERBOSE("readNumber: hex literal");
        num += advance(state); // consume 'x'
        while (isxdigit(peek(state)) || peek(state) == '_')
            num += advance(state);
        LUC_LOG_LEXER("readNumber: hex literal: " << num);
        return makeToken(state, TokenType::HEX_LITERAL, num);
    }

    // Binary: 0b1010
    if (first == '0' && (peek(state) == 'b' || peek(state) == 'B')) {
        LUC_LOG_LEXER_VERBOSE("readNumber: binary literal");
        num += advance(state); // consume 'b'
        while (peek(state) == '0' || peek(state) == '1' || peek(state) == '_')
            num += advance(state);
        LUC_LOG_LEXER("readNumber: binary literal: " << num);
        return makeToken(state, TokenType::BINARY_LITERAL, num);
    }

    // Integer or float
    bool isFloat = false;
    while (isdigit(peek(state)) || peek(state) == '_')
        num += advance(state);

    if (peek(state) == '.' && peekNext(state) != '.') {
        isFloat = true;
        LUC_LOG_LEXER_VERBOSE("readNumber: decimal point detected");
        num += advance(state); // consume '.'
        while (isdigit(peek(state)) || peek(state) == '_')
            num += advance(state);
    }

    // Exponent: 1e10, 1.5e-3
    if (peek(state) == 'e' || peek(state) == 'E') {
        isFloat = true;
        LUC_LOG_LEXER_VERBOSE("readNumber: exponent detected");
        num += advance(state);
        if (peek(state) == '+' || peek(state) == '-')
            num += advance(state);
        while (isdigit(peek(state)))
            num += advance(state);
    }

    LUC_LOG_LEXER("readNumber: " << (isFloat ? "float" : "int") << " literal: " << num);
    return makeToken(state, isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL, num);
}

static Token readString(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("readString: starting");
    std::string str;
    while (!isAtEnd(state) && peek(state) != '"') {
        if (peek(state) == '\n') {
            state->line++;
            state->column = 1;
        }
        if (peek(state) == '\\') {
            advance(state); // consume '\'
            char esc = advance(state);
            LUC_LOG_LEXER_EXTREME("readString: escape sequence '\\" << esc << "'");
            switch (esc) {
            case 'n':
                str += '\n';
                break;
            case 't':
                str += '\t';
                break;
            case 'r':
                str += '\r';
                break;
            case '"':
                str += '"';
                break;
            case '\\':
                str += '\\';
                break;
            case '\'':
                str += '\'';
                break;
            case '0':
                str += '\0';
                break;
            case 'x': {
                std::string hex;
                for (int i = 0; i < 2 && isxdigit(peek(state)); i++)
                    hex += advance(state);
                str += (char)std::stoi(hex, nullptr, 16);
                break;
            }
            case 'u': {
                std::string hex;
                for (int i = 0; i < 4 && isxdigit(peek(state)); i++)
                    hex += advance(state);
                unsigned long cp = std::stoul(hex, nullptr, 16);
                // UTF-8 encoding...
                if (cp < 0x80) {
                    str += (char)cp;
                } else if (cp < 0x800) {
                    str += (char)(0xC0 | (cp >> 6));
                    str += (char)(0x80 | (cp & 0x3F));
                } else {
                    str += (char)(0xE0 | (cp >> 12));
                    str += (char)(0x80 | ((cp >> 6) & 0x3F));
                    str += (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            case 'U': {
                std::string hex;
                for (int i = 0; i < 8 && isxdigit(peek(state)); i++)
                    hex += advance(state);
                unsigned long cp = std::stoul(hex, nullptr, 16);
                // UTF-8 encoding for up to 4 bytes...
                if (cp < 0x80) {
                    str += (char)cp;
                } else if (cp < 0x800) {
                    str += (char)(0xC0 | (cp >> 6));
                    str += (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    str += (char)(0xE0 | (cp >> 12));
                    str += (char)(0x80 | ((cp >> 6) & 0x3F));
                    str += (char)(0x80 | (cp & 0x3F));
                } else {
                    str += (char)(0xF0 | (cp >> 18));
                    str += (char)(0x80 | ((cp >> 12) & 0x3F));
                    str += (char)(0x80 | ((cp >> 6) & 0x3F));
                    str += (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            default:
                str += '\\';
                str += esc;
                break;
            }
        } else {
            str += advance(state);
        }
    }
    if (!isAtEnd(state))
        advance(state); // consume closing '"'
    
    LUC_LOG_LEXER_VERBOSE("readString: result length=" << str.size());
    return makeToken(state, TokenType::STRING_LITERAL, str);
}

static Token readChar(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("readChar: starting");
    std::string result;
    bool closed = false;

    while (!isAtEnd(state)) {
        char c = peek(state);
        if (c == '\'') {
            advance(state);          // consume closing '
            closed = true;
            break;
        }
        if (c == '\\') {
            advance(state);          // consume backslash
            char esc = advance(state);
            switch (esc) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '\'': result += '\''; break;
                case '0':  result += '\0'; break;
                case 'x': {
                    std::string hex;
                    for (int i = 0; i < 2 && isxdigit(peek(state)); ++i)
                        hex += advance(state);
                    if (hex.size() != 2) {
                        // incomplete hex escape – error recovery
                        return makeToken(state, TokenType::UNKNOWN, "");
                    }
                    unsigned long val = std::stoul(hex, nullptr, 16);
                    result += static_cast<char>(val);
                    break;
                }
                case 'u': {
                    std::string hex;
                    for (int i = 0; i < 4 && isxdigit(peek(state)); ++i)
                        hex += advance(state);
                    if (hex.size() != 4) {
                        return makeToken(state, TokenType::UNKNOWN, "");
                    }
                    unsigned long cp = std::stoul(hex, nullptr, 16);
                    // UTF‑8 encoding (same as string)
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (cp >> 18));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                case 'U': {
                    std::string hex;
                    for (int i = 0; i < 8 && isxdigit(peek(state)); ++i)
                        hex += advance(state);
                    if (hex.size() != 8) {
                        return makeToken(state, TokenType::UNKNOWN, "");
                    }
                    unsigned long cp = std::stoul(hex, nullptr, 16);
                    // UTF‑8 encoding (same as string)
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (cp >> 18));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    // unknown escape – keep as two characters (error may be reported later)
                    result += '\\';
                    result += esc;
                    break;
            }
        } else {
            // normal character (not a backslash)
            result += advance(state);
        }
    }

    if (!closed) {
        // unterminated character literal
        return makeToken(state, TokenType::UNKNOWN, "");
    }

    return makeToken(state, TokenType::CHAR_LITERAL, result);
}

static Token readRawString(LexerState* state, int hashCount) {
    LUC_LOG_LEXER_VERBOSE("readRawString: starting with " << hashCount << " hash delimiters");
    std::string str;
    while (!isAtEnd(state)) {
        if (peek(state) == '"') {
            // Check if the next hashCount characters are '#' and then end
            bool match = true;
            for (int i = 0; i < hashCount; ++i) {
                if (peekNextOffset(state, i + 1) != '#') {
                    match = false;
                    break;
                }
            }
            if (match) {
                advance(state); // consume "
                for (int i = 0; i < hashCount; ++i) advance(state); // consume the #s
                break;
            }
        }
        if (peek(state) == '\n') {
            state->line++;
            state->column = 1;
        }
        str += advance(state);
        if (isAtEnd(state)) {
            // Unterminated raw string – error recovery
            break;
        }
    }
    return makeToken(state, TokenType::RAW_STRING_LITERAL, str);
}

static Token readDocComment(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("readDocComment: starting");
    advance(state); // consume first '-' (of opening --)
    advance(state); // consume second '-'

    std::string doc;

    while (!isAtEnd(state)) {
        // Closing sequence is --/
        if (peek(state) == '-' && state->pos + 1 < state->src.size() && state->src[state->pos + 1] == '-' &&
            state->pos + 2 < state->src.size() && state->src[state->pos + 2] == '/') {
            advance(state);
            advance(state);
            advance(state); // consume --/
            break;
        }
        if (peek(state) == '\n') {
            state->line++;
            state->column = 1;
        }
        doc += advance(state);
    }

    LUC_LOG_LEXER_VERBOSE("readDocComment: length=" << doc.size());
    return makeToken(state, TokenType::DOC_COMMENT, doc);
}

static Token readLineComment(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("readLineComment: starting");
    advance(state); // consume the second '-'

    // Strip a single optional leading space (the common "-- text" style).
    if (peek(state) == ' ')
        advance(state);

    std::string text;
    while (!isAtEnd(state) && peek(state) != '\n')
        text += advance(state);

    LUC_LOG_LEXER_VERBOSE("readLineComment: '" << text << "'");
    return makeToken(state, TokenType::LINE_COMMENT, text);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scanner
// ─────────────────────────────────────────────────────────────────────────────
static Token getNextToken(LexerState* state) {
    LUC_LOG_LEXER_VERBOSE("getNextToken: pos=" << state->pos << ", line=" << state->line << ", col=" << state->column);
    
    skipWhitespace(state);
    if (isAtEnd(state)) {
        LUC_LOG_LEXER("getNextToken: EOF");
        return makeToken(state, TokenType::EOF_TOKEN, "EOF");
    }

    char c = advance(state);

    // ── Reject non‑ASCII in source code (except inside comments/literals) ──
    if (static_cast<unsigned char>(c) >= 0x80) {
        // Found a non‑ASCII byte – consume the whole UTF‑8 sequence
        std::string badChar(1, c);
        int len = utf8SequenceLength(static_cast<unsigned char>(c));
        for (int i = 1; i < len && !isAtEnd(state); ++i) {
            badChar += advance(state);
        }
        // Report a single error at the start location
        diagnostic::error(
            DiagnosticCategory::Lexical,
            state->fileName,
            SourceLocation(state->line, state->column - len),  // start column
            DiagCode::E1001,
            {badChar}
        );
        // Continue scanning (the bad char is consumed)
        return getNextToken(state); // recurse to next token
    }

    LUC_LOG_LEXER_EXTREME("getNextToken: processing char '" << c << "'");

    // ── Identifiers & Keywords ─────────────────────────────────────────────────
    if (isalpha(c) || c == '_') {
        std::string ident(1, c);
        while (isalnum(peek(state)) || peek(state) == '_')
            ident += advance(state);

        // r"..." raw string literal with optional # delimiters
        if (ident == "r") {
            int hashCount = 0;
            while (peek(state) == '#') {
                ++hashCount;
                advance(state);
            }
            if (peek(state) == '"') {
                advance(state); // consume opening "
                return readRawString(state, hashCount);
            }
        }

        // Standalone _ is a wildcard token
        if (ident == "_") {
            LUC_LOG_LEXER_EXTREME("getNextToken: wildcard '_'");
            return makeToken(state, TokenType::WILDCARD, "_");
        }

        auto it = state->keywords.find(ident);
        if (it != state->keywords.end()) {
            LUC_LOG_LEXER_VERBOSE("getNextToken: keyword '" << ident << "'");
            return makeToken(state, it->second, ident);
        }
        LUC_LOG_LEXER_VERBOSE("getNextToken: identifier '" << ident << "'");
        return makeToken(state, TokenType::IDENTIFIER, ident);
    }

    // ── Numbers ────────────────────────────────────────────────────────────────
    if (isdigit(c)) {
        return readNumber(state, c);
    }

    // ── String literals ────────────────────────────────────────────────────────
    if (c == '"') {
        LUC_LOG_LEXER_VERBOSE("getNextToken: string literal");
        return readString(state);
    }

    // ── Char literals ──────────────────────────────────────────────────────────
    if (c == '\'') {
        LUC_LOG_LEXER_VERBOSE("getNextToken: char literal");
        return readChar(state);
    }

    // ── Operators & Symbols ────────────────────────────────────────────────────
    switch (c) {
    // ── Access ─────────────────────────────────────────────────────────────────
    case '.':
        if (match(state, '.')) {
            if (match(state, '.')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '...' (variadic)");
                return makeToken(state, TokenType::VARIADIC, "...");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '..' (range)");
            return makeToken(state, TokenType::RANGE, "..");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '.'");
        return makeToken(state, TokenType::DOT, ".");

    case ':':
        LUC_LOG_LEXER_EXTREME("getNextToken: ':'");
        return makeToken(state, TokenType::COLON, ":");

    case '?':
        if (match(state, '.')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '?.'");
            return makeToken(state, TokenType::QUESTION_DOT, "?.");
        }
        if (match(state, '?')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '\?\?'");
            return makeToken(state, TokenType::QUESTION_QUESTION, "??");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '?'");
        return makeToken(state, TokenType::QUESTION, "?");

    case '=':
        if (match(state, '=')) {
            if (match(state, '=')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '==='");
                return makeToken(state, TokenType::EQUAL_EQUAL_EQUAL, "===");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '=='");
            return makeToken(state, TokenType::EQUAL_EQUAL, "==");
        }
        if (peek(state) == '>') {
            advance(state);
            return Token{TokenType::FAT_ARROW, "=>", state->line, state->column};
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '='");
        return makeToken(state, TokenType::ASSIGN, "=");

    case '!':
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '!='");
            return makeToken(state, TokenType::NOT_EQUAL, "!=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '!'");
        return makeToken(state, TokenType::BANG, "!");

    case '<':
        if (match(state, '<')) {
            if (match(state, '=')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '<<='");
                return makeToken(state, TokenType::SHL_ASSIGN, "<<=");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '<<'");
            return makeToken(state, TokenType::SHL, "<<");
        }
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '<='");
            return makeToken(state, TokenType::LESS_EQUAL, "<=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '<'");
        return makeToken(state, TokenType::LESS, "<");

    case '>':
        if (match(state, '>')) {
            if (match(state, '=')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '>>='");
                return makeToken(state, TokenType::SHR_ASSIGN, ">>=");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '>>'");
            return makeToken(state, TokenType::SHR, ">>");
        }
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '>='");
            return makeToken(state, TokenType::GREATER_EQUAL, ">=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '>'");
        return makeToken(state, TokenType::GREATER, ">");

    case '+':
        if (match(state, '>')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '+>'");
            return makeToken(state, TokenType::COMPOSE, "+>");
        }
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '+='");
            return makeToken(state, TokenType::PLUS_ASSIGN, "+=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '+'");
        return makeToken(state, TokenType::PLUS, "+");

    case '-':
        if (peek(state) == '-') {
            LUC_LOG_LEXER_EXTREME("getNextToken: line comment '--'");
            return readLineComment(state);
        }
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '-='");
            return makeToken(state, TokenType::MINUS_ASSIGN, "-=");
        }
        if (match(state, '>')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '->'");
            return makeToken(state, TokenType::ARROW, "->");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '-'");
        return makeToken(state, TokenType::MINUS, "-");

    case '*':
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '*='");
            return makeToken(state, TokenType::MUL_ASSIGN, "*=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '*'");
        return makeToken(state, TokenType::MUL, "*");

    case '/':
        if (peek(state) == '-' && state->pos + 1 < state->src.size() && state->src[state->pos + 1] == '-') {
            LUC_LOG_LEXER_EXTREME("getNextToken: doc comment '/--'");
            return readDocComment(state);
        }
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '/='");
            return makeToken(state, TokenType::DIV_ASSIGN, "/=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '/'");
        return makeToken(state, TokenType::DIV, "/");

    case '%':
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '%='");
            return makeToken(state, TokenType::MOD_ASSIGN, "%=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '%'");
        return makeToken(state, TokenType::MOD, "%");

    case '^':
        if (match(state, '=')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '^='");
            return makeToken(state, TokenType::POW_ASSIGN, "^=");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '^'");
        return makeToken(state, TokenType::POW, "^");

    case '&':
        if (match(state, '&')) {
            if (match(state, '=')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '&&='");
                return makeToken(state, TokenType::BIT_AND_ASSIGN, "&&=");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '&&'");
            return makeToken(state, TokenType::BIT_AND, "&&");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '&'");
        return makeToken(state, TokenType::AMPERSAND, "&");

    case '|':
        if (match(state, '>')) {
            LUC_LOG_LEXER_EXTREME("getNextToken: '|>' (pipeline)");
            return makeToken(state, TokenType::PIPELINE, "|>");
        }
        if (match(state, '|')) {
            if (match(state, '=')) {
                LUC_LOG_LEXER_EXTREME("getNextToken: '||='");
                return makeToken(state, TokenType::BIT_OR_ASSIGN, "||=");
            }
            LUC_LOG_LEXER_EXTREME("getNextToken: '||'");
            return makeToken(state, TokenType::BIT_OR, "||");
        }
        LUC_LOG_LEXER_EXTREME("getNextToken: '|'");
        return makeToken(state, TokenType::PIPE, "|");

    case '~':
        if (match(state, '~')) {
            return makeToken(state, TokenType::BIT_NOT, "~~");
        }
        if (match(state, '^')) {
            if (match(state, '=')) {
                return makeToken(state, TokenType::BIT_XOR_ASSIGN, "~^=");
            }
            return makeToken(state, TokenType::BIT_XOR, "~^");
        }
        return makeToken(state, TokenType::TILDE, "~");
        
    case '@':
        LUC_LOG_LEXER_EXTREME("getNextToken: '@'");
        return makeToken(state, TokenType::AT_SIGN, "@");
    case '#':
        LUC_LOG_LEXER_EXTREME("getNextToken: '#'");
        return makeToken(state, TokenType::HASH, "#");

    case ',':
        LUC_LOG_LEXER_EXTREME("getNextToken: ','");
        return makeToken(state, TokenType::COMMA, ",");
    case ';':
        LUC_LOG_LEXER_EXTREME("getNextToken: ';'");
        return makeToken(state, TokenType::SEMICOLON, ";");
    case '(':
        LUC_LOG_LEXER_EXTREME("getNextToken: '('");
        return makeToken(state, TokenType::LPAREN, "(");
    case ')':
        LUC_LOG_LEXER_EXTREME("getNextToken: ')'");
        return makeToken(state, TokenType::RPAREN, ")");
    case '{':
        LUC_LOG_LEXER_EXTREME("getNextToken: '{'");
        return makeToken(state, TokenType::LBRACE, "{");
    case '}':
        LUC_LOG_LEXER_EXTREME("getNextToken: '}'");
        return makeToken(state, TokenType::RBRACE, "}");
    case '[':
        LUC_LOG_LEXER_EXTREME("getNextToken: '['");
        return makeToken(state, TokenType::LBRACKET, "[");
    case ']':
        LUC_LOG_LEXER_EXTREME("getNextToken: ']'");
        return makeToken(state, TokenType::RBRACKET, "]");
    }

    // Unknown character
    LUC_LOG_LEXER("getNextToken: UNKNOWN character '" << c << "'");
    return makeToken(state, TokenType::UNKNOWN, std::string(1, c));
}

// ─────────────────────────────────────────────────────────────────────────────
// Public tokenize
// ─────────────────────────────────────────────────────────────────────────────
std::vector<Token> tokenize(LexerState* state) {
    LUC_LOG_LEXER("tokenize: starting");
    std::vector<Token> tokens;
    Token t = makeToken(state, TokenType::EOF_TOKEN, "");
    int tokenCount = 0;

    do {
        t = getNextToken(state);
        tokens.push_back(t);
        tokenCount++;
        LUC_LOG_LEXER_EXTREME("tokenize: token " << tokenCount << " - type=" << static_cast<int>(t.type) << ", value='" << t.value << "'");
    } while (t.type != TokenType::EOF_TOKEN);

    LUC_LOG_LEXER("tokenize: complete, " << tokenCount << " tokens produced");
    return tokens;
}