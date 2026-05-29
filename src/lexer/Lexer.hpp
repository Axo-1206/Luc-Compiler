/**
 * @file Lexer.h
 *
 * @responsibility The Front-End Scanner. Converts raw source code (strings) into a Token stream.
 * 
 * @related_files
 *   - src/Tokens.hpp (The vocabulary: Defines what a Token is)
 *   - src/lexer/Lexer.cpp (The implementation: Heavy regex/scanning logic)
 *   - src/parser/Parser.hpp (The consumer: Reads the tokens produced here)
 * 
 * @spec_references docs/LUC_GRAMMAR.md (Legal identifiers, literal formats)
 * 
 * @note This Lexer is responsible for emitting DOC_COMMENT tokens (started with /--) 
 *       which the Parser later attaches to AST nodes.
 */

#pragma once
#include "Tokens.hpp"
#include "Diagnostics/Diagnostic.hpp"

#include <string>
#include <vector>

// Forward declaration of lexer state (opaque pointer pattern)
struct LexerState;

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle Management
// ─────────────────────────────────────────────────────────────────────────────
LexerState* createLexer(const std::string& source, InternedString fileName);
void destroyLexer(LexerState* state);

// Returns all tokens including the final EOF_TOKEN.
std::vector<Token> tokenize(LexerState* state);

// ─────────────────────────────────────────────────────────────────────────────
// Accessors (for debugging)
// ─────────────────────────────────────────────────────────────────────────────
int getLexerLine(const LexerState* state);
int getLexerColumn(const LexerState* state);
size_t getLexerPosition(const LexerState* state);