#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "AST.h"
#include "Token.h"

enum class Cat { Plain, Keyword, Type, Func, Var, Number, String, Operator, Comment, Error };

// AST-driven highlighter. Pass 1 (constructor) walks the AST and records the
// ROLE of identifier / type tokens by source position; pass 2 walks the full
// token stream (incl. whitespace and comments) and renders it.
// Tokens are never dropped, so the output always reproduces the source exactly.
class Highlighter {
public:
    // `diags` (lexer + parser errors) are shown too: the token at each error position
    // is underlined in red (tooltip = message) and HTML gets a diagnostics panel.
    explicit Highlighter(const Node& astRoot, std::vector<Diagnostic> diags = {});

    Cat classify(const Token& t) const;
    std::string toAnsi(const std::vector<Token>& tokens) const;
    std::string toHtml(const std::vector<Token>& tokens, const std::string& title) const;

private:
    void collect(const Node& n);
    std::map<std::pair<int, int>, Cat> roles_;   // (line, col) -> role from the AST
    std::vector<Diagnostic> diags_;
    std::map<std::pair<int, int>, std::string> errAt_;   // (line, col) -> messages joined by '\n'
};
