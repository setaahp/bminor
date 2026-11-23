#pragma once
#include <string>

enum class TokType {
    Keyword, Ident, IntLit, BoolLit, StringLit, CharLit,
    Operator, Delimiter, LineComment, BlockComment,
    Whitespace, Invalid, Eof
};

const char* tokTypeName(TokType t);

struct Token {
    TokType type;
    std::string lexeme;   // exact substring of the source
    std::string file;
    int line;             // 1-based
    int col;              // 1-based (bytes)

    // Whitespace and comments are kept by the lexer (so the highlighter can
    // reproduce the source exactly) but the parser never sees them.
    bool isTrivia() const {
        return type == TokType::Whitespace || type == TokType::LineComment ||
               type == TokType::BlockComment;
    }
};

struct Diagnostic {
    std::string phase;    // "lexical" or "syntax"
    std::string message;
    std::string file;
    int line;
    int col;
    std::string str() const;
};
