#include "Lexer.h"
#include <cctype>
#include <cstdio>
#include <unordered_set>

namespace {

const std::unordered_set<std::string> kKeywords = {
    "array", "boolean", "char", "else", "for", "function", "if", "integer",
    "print", "return", "string", "void", "while"};

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentPart(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool isDigit(char c)      { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

}  // namespace

Lexer::Lexer(std::string source, std::string fileName)
    : src_(std::move(source)), file_(std::move(fileName)) {}

char Lexer::peek(size_t k) const {
    return pos_ + k < src_.size() ? src_[pos_ + k] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
    return c;
}

void Lexer::emit(TokType t, size_t start, int line, int col) {
    out_.push_back(Token{t, src_.substr(start, pos_ - start), file_, line, col});
}

void Lexer::report(const std::string& msg, int line, int col) {
    errors_.push_back(Diagnostic{"lexical", msg, file_, line, col});
}

std::vector<Token> Lexer::tokenize() {
    while (!atEnd()) {
        size_t start = pos_;
        int line = line_, col = col_;
        char c = peek();

        if (std::isspace(static_cast<unsigned char>(c))) {
            while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) advance();
            emit(TokType::Whitespace, start, line, col);
        } else if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
            emit(TokType::LineComment, start, line, col);
        } else if (c == '/' && peek(1) == '*') {
            scanBlockComment(start, line, col);
        } else if (isIdentStart(c)) {
            scanIdentifier(start, line, col);
        } else if (isDigit(c)) {
            scanNumber(start, line, col);
        } else if (c == '"') {
            scanString(start, line, col);
        } else if (c == '\'') {
            scanChar(start, line, col);
        } else if (!scanOperator(start, line, col)) {
            // Unknown character: emit INVALID, report, skip it, keep going.
            advance();
            std::string shown;
            if (std::isprint(static_cast<unsigned char>(c))) {
                shown = std::string(1, c);
            } else {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\x%02X", static_cast<unsigned char>(c));
                shown = buf;
            }
            report("unexpected character '" + shown + "'", line, col);
            emit(TokType::Invalid, start, line, col);
        }
    }
    out_.push_back(Token{TokType::Eof, "", file_, line_, col_});
    return out_;
}

void Lexer::scanBlockComment(size_t start, int line, int col) {
    advance(); advance();  // "/*"
    bool closed = false;
    while (!atEnd()) {
        if (peek() == '*' && peek(1) == '/') { advance(); advance(); closed = true; break; }
        advance();
    }
    if (!closed) {
        report("unterminated block comment", line, col);
        emit(TokType::Invalid, start, line, col);
    } else {
        emit(TokType::BlockComment, start, line, col);
    }
}

void Lexer::scanIdentifier(size_t start, int line, int col) {
    while (!atEnd() && isIdentPart(peek())) advance();
    std::string text = src_.substr(start, pos_ - start);
    if (text.size() > 256) report("identifier longer than 256 characters", line, col);
    TokType t = TokType::Ident;
    if (text == "true" || text == "false") t = TokType::BoolLit;
    else if (kKeywords.count(text)) t = TokType::Keyword;   // keyword beats identifier
    emit(t, start, line, col);
}

void Lexer::scanNumber(size_t start, int line, int col) {
    // A sign is NOT part of the literal: "n-1" must be IDENT OP INT.
    // Unary minus is handled by the parser.
    while (!atEnd() && isDigit(peek())) advance();
    std::string digits = src_.substr(start, pos_ - start);
    size_t nz = digits.find_first_not_of('0');
    digits = (nz == std::string::npos) ? "0" : digits.substr(nz);
    static const std::string kMax = "9223372036854775808";   // 2^63 (magnitude of the most negative integer)
    if (digits.size() > kMax.size() || (digits.size() == kMax.size() && digits > kMax))
        report("integer literal does not fit in a 64-bit integer", line, col);
    emit(TokType::IntLit, start, line, col);
}

void Lexer::scanString(size_t start, int line, int col) {
    advance();  // opening quote
    bool closed = false;
    int length = 0;                                 // characters after escape processing
    while (!atEnd()) {
        char d = peek();
        if (d == '"') { advance(); closed = true; break; }
        if (d == '\n') break;                       // strings cannot span lines
        if (d == '\\') {                            // backslash + ANY char = that char (\n, \0 special)
            advance();
            if (atEnd() || peek() == '\n') break;
            advance();
            ++length;
            continue;
        }
        advance();
        ++length;
    }
    if (!closed) {
        report("unterminated string literal", line, col);
        emit(TokType::Invalid, start, line, col);
    } else {
        if (length > 256) report("string literal longer than 256 characters", line, col);
        emit(TokType::StringLit, start, line, col);
    }
}

void Lexer::scanChar(size_t start, int line, int col) {
    advance();  // opening '
    bool ok = false;
    if (!atEnd() && peek() != '\n' && peek() != '\'') {
        bool valid = true;
        if (peek() == '\\') {
            advance();
            if (!atEnd() && peek() != '\n') advance(); else valid = false;
        } else {
            valid = static_cast<unsigned char>(peek()) < 0x80;   // ASCII only
            advance();
        }
        if (valid && peek() == '\'') { advance(); ok = true; }
    }
    if (!ok) {
        while (!atEnd() && peek() != '\n' && peek() != '\'') advance();
        if (peek() == '\'') advance();
        report("invalid character literal (expected one ASCII character or an escape)", line, col);
        emit(TokType::Invalid, start, line, col);
    } else {
        emit(TokType::CharLit, start, line, col);
    }
}

bool Lexer::scanOperator(size_t start, int line, int col) {
    static const char* two[] = {"==", "!=", "<=", ">=", "&&", "||", "++", "--"};
    for (const char* op : two) {                    // longest match: try 2 chars first
        if (peek() == op[0] && peek(1) == op[1]) {
            advance(); advance();
            emit(TokType::Operator, start, line, col);
            return true;
        }
    }
    static const std::string ops1 = "+-*/%^!<>=";
    static const std::string delims = "()[]{},:;";
    char c = peek();
    if (ops1.find(c) != std::string::npos) {
        advance();
        emit(TokType::Operator, start, line, col);
        return true;
    }
    if (delims.find(c) != std::string::npos) {
        advance();
        emit(TokType::Delimiter, start, line, col);
        return true;
    }
    return false;   // note: a lone '&' or '|' ends up as INVALID
}
