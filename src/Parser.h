#pragma once
#include <string>
#include <vector>
#include "AST.h"
#include "Token.h"

// Recursive-descent (LL) parser. One method per non-terminal of docs/GRAMMAR.md.
// Errors: panic-mode recovery (throw ParseError -> synchronize()) plus "soft"
// expectations for closing tokens ) ] ; which report and keep going.
class Parser {
public:
    explicit Parser(const std::vector<Token>& allTokens);
    NodePtr parseProgram();
    const std::vector<Diagnostic>& errors() const { return errors_; }

private:
    // token helpers
    const Token& peek(size_t k = 0) const;
    Token advance();
    bool atEnd() const;
    bool is(const char* lexeme, size_t k = 0) const;
    bool match(const char* lexeme);
    void expect(const char* lexeme);        // hard: error + throw
    bool expectSoft(const char* lexeme);    // soft: error, no throw, no consume
    void error(const Token& at, const std::string& msg);
    [[noreturn]] void fail(const std::string& msg);
    static std::string describe(const Token& t);
    void synchronize(bool inBlock);

    // grammar
    NodePtr parseDeclaration();
    NodePtr parseType();
    NodePtr parseParam();
    NodePtr parseArrayInit();
    NodePtr parseBlock();
    NodePtr parseStatementSafe();
    NodePtr parseStatement();
    NodePtr parseOptExpr(const char* stop);
    NodePtr parseExpr();
    NodePtr parseAssignment();
    NodePtr parseLevel(size_t level);
    NodePtr parsePower();
    NodePtr parseUnary();
    NodePtr parsePostfix();
    NodePtr parsePrimary();

    std::vector<Token> toks_;
    size_t p_ = 0;
    std::vector<Diagnostic> errors_;
};
