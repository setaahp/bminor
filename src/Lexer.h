#pragma once
#include <string>
#include <vector>
#include "Token.h"

// Hand-written scanner. Each scanXxx() method is the code form of one DFA
// (see docs/LEXICAL.md). Longest-match is obtained by always consuming as
// many characters as the current DFA allows; keywords are separated from
// identifiers by a table lookup after the identifier DFA finishes.
class Lexer {
public:
    Lexer(std::string source, std::string fileName);

    // Returns ALL tokens (including whitespace/comments/INVALID), ending with EOF.
    std::vector<Token> tokenize();
    const std::vector<Diagnostic>& errors() const { return errors_; }

private:
    bool atEnd() const { return pos_ >= src_.size(); }
    char peek(size_t k = 0) const;
    char advance();
    void emit(TokType t, size_t start, int line, int col);
    void report(const std::string& msg, int line, int col);

    void scanBlockComment(size_t start, int line, int col);
    void scanIdentifier(size_t start, int line, int col);
    void scanNumber(size_t start, int line, int col);
    void scanString(size_t start, int line, int col);
    void scanChar(size_t start, int line, int col);
    bool scanOperator(size_t start, int line, int col);

    std::string src_, file_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1;
    std::vector<Token> out_;
    std::vector<Diagnostic> errors_;
};
