#include "Token.h"

const char* tokTypeName(TokType t) {
    switch (t) {
        case TokType::Keyword:      return "KEYWORD";
        case TokType::Ident:        return "IDENT";
        case TokType::IntLit:       return "INT";
        case TokType::BoolLit:      return "BOOL";
        case TokType::StringLit:    return "STRING";
        case TokType::CharLit:      return "CHAR";
        case TokType::Operator:     return "OP";
        case TokType::Delimiter:    return "DELIM";
        case TokType::LineComment:  return "LINE_COMMENT";
        case TokType::BlockComment: return "BLOCK_COMMENT";
        case TokType::Whitespace:   return "WS";
        case TokType::Invalid:      return "INVALID";
        case TokType::Eof:          return "EOF";
    }
    return "?";
}

std::string Diagnostic::str() const {
    return file + ":" + std::to_string(line) + ":" + std::to_string(col) +
           ": " + phase + " error: " + message;
}
