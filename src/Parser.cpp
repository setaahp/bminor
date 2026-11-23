#include "Parser.h"
#include <algorithm>

namespace {

struct ParseError {};

NodePtr mk(NK k, const std::string& text, const Token& t) {
    return std::make_unique<Node>(k, text, t.line, t.col);
}
NodePtr mkAt(NK k, const std::string& text, const Node& n) {
    return std::make_unique<Node>(k, text, n.line, n.col);
}

// Binary operator levels, lowest precedence first. All are left-associative.
// (Assignment is handled above these, '^' and unary below them.)
const std::vector<std::vector<std::string>> kLevels = {
    {"&&", "||"},                       // same row in the course's operator table
    {"<", "<=", ">", ">=", "==", "!="},
    {"+", "-"},
    {"*", "/", "%"},
};

}  // namespace

Parser::Parser(const std::vector<Token>& all) {
    // The parser never sees whitespace/comments. INVALID tokens were already
    // reported by the lexer, so they are skipped too (lexer-level recovery).
    // A malformed string/char literal still occupies an operand position, so it is
    // kept as a literal (the lexer already reported it); this avoids cascading errors.
    for (const auto& t : all) {
        if (t.isTrivia()) continue;
        if (t.type == TokType::Invalid) {
            Token fixed = t;
            if (!t.lexeme.empty() && t.lexeme[0] == '"') fixed.type = TokType::StringLit;
            else if (!t.lexeme.empty() && t.lexeme[0] == '\'') fixed.type = TokType::CharLit;
            else continue;                      // stray char / unterminated comment: skip
            toks_.push_back(fixed);
        } else {
            toks_.push_back(t);
        }
    }
    if (toks_.empty() || toks_.back().type != TokType::Eof)
        toks_.push_back(Token{TokType::Eof, "", "", 1, 1});
}

// ---------- helpers ----------
const Token& Parser::peek(size_t k) const { return toks_[std::min(p_ + k, toks_.size() - 1)]; }
bool Parser::atEnd() const { return peek().type == TokType::Eof; }
Token Parser::advance() {
    Token t = peek();
    if (p_ + 1 < toks_.size()) ++p_;
    return t;
}
bool Parser::is(const char* lexeme, size_t k) const { return peek(k).lexeme == lexeme; }
bool Parser::match(const char* lexeme) {
    if (is(lexeme)) { advance(); return true; }
    return false;
}
std::string Parser::describe(const Token& t) {
    return t.type == TokType::Eof ? "end of file" : "'" + t.lexeme + "'";
}
void Parser::error(const Token& at, const std::string& msg) {
    errors_.push_back(Diagnostic{"syntax", msg, at.file, at.line, at.col});
}
void Parser::fail(const std::string& msg) {
    error(peek(), msg);
    throw ParseError{};
}
void Parser::expect(const char* lexeme) {
    if (!match(lexeme))
        fail(std::string("expected '") + lexeme + "', got " + describe(peek()));
}
bool Parser::expectSoft(const char* lexeme) {
    if (match(lexeme)) return true;
    error(peek(), std::string("expected '") + lexeme + "' before " + describe(peek()));
    return false;
}

// Panic mode: skip tokens until a synchronization point.
void Parser::synchronize(bool inBlock) {
    int depth = 0;   // braces opened while skipping: a skipped { ... } group is skipped as a whole
    while (!atEnd()) {
        if (depth == 0) {
            if (is(";")) { advance(); return; }                         // end of statement
            if (is("}")) { if (inBlock) return; advance(); continue; }  // end of enclosing block
            if (is("if") || is("for") || is("return") || is("print")) return;
            if (peek().type == TokType::Ident && is(":", 1)) return;    // next declaration
        } else if (is("}")) {
            --depth; advance(); continue;
        }
        if (is("{")) ++depth;
        advance();
    }
}

// ---------- declarations ----------
NodePtr Parser::parseProgram() {
    auto prog = mk(NK::Program, "", peek());
    while (!atEnd()) {
        size_t start = p_;
        try {
            prog->add(parseDeclaration());
        } catch (ParseError&) {
            synchronize(false);
            if (p_ == start && !atEnd()) advance();   // guarantee progress
            prog->add(mk(NK::Error, "", toks_[start]));
        }
    }
    return prog;
}

NodePtr Parser::parseDeclaration() {
    if (peek().type != TokType::Ident)
        fail("expected a declaration (name ':' type), got " + describe(peek()));
    Token name = advance();
    expect(":");

    if (is("function")) {
        advance();
        auto fn = mk(NK::FuncDecl, name.lexeme, name);
        fn->add(parseType());
        expect("(");
        if (!is(")")) {
            do { fn->add(parseParam()); } while (match(","));
        }
        expectSoft(")");
        if (match(";")) return fn;           // prototype
        expect("=");
        fn->add(parseBlock());
        return fn;
    }

    auto var = mk(NK::VarDecl, name.lexeme, name);
    var->add(parseType());
    if (match("=")) {
        if (is("{")) var->add(parseArrayInit());
        else var->add(parseExpr());
    }
    expectSoft(";");
    return var;
}

NodePtr Parser::parseType() {
    Token t = peek();
    if (is("array")) {
        advance();
        expect("[");
        NodePtr size;
        if (!is("]")) size = parseExpr();
        expectSoft("]");
        auto n = mk(NK::ArrayType, "array", t);
        n->add(parseType());
        if (size) n->add(std::move(size));
        return n;
    }
    if (is("integer") || is("boolean") || is("char") || is("string") || is("void")) {
        advance();
        return mk(NK::TypeName, t.lexeme, t);
    }
    fail("expected a type, got " + describe(t));
}

NodePtr Parser::parseParam() {
    if (peek().type != TokType::Ident)
        fail("expected a parameter name, got " + describe(peek()));
    Token name = advance();
    expect(":");
    auto p = mk(NK::Param, name.lexeme, name);
    p->add(parseType());
    return p;
}

NodePtr Parser::parseArrayInit() {
    Token open = peek();
    expect("{");
    auto n = mk(NK::ArrayInit, "", open);
    if (!is("}")) {
        do { n->add(parseExpr()); } while (match(","));
    }
    expect("}");
    return n;
}

// ---------- statements ----------
NodePtr Parser::parseBlock() {
    Token open = peek();
    expect("{");
    auto b = mk(NK::Block, "", open);
    while (!is("}") && !atEnd()) b->add(parseStatementSafe());
    expect("}");
    return b;
}

NodePtr Parser::parseStatementSafe() {
    size_t start = p_;
    try {
        return parseStatement();
    } catch (ParseError&) {
        synchronize(true);
        if (p_ == start && !atEnd() && !is("}")) advance();   // guarantee progress
        return mk(NK::Error, "", toks_[start]);
    }
}

NodePtr Parser::parseOptExpr(const char* stop) {
    if (is(stop)) return mk(NK::Empty, "", peek());
    return parseExpr();
}

NodePtr Parser::parseStatement() {
    if (peek().type == TokType::Ident && is(":", 1)) {                           // the LL(2) point
        if (is("function", 2)) error(peek(), "functions cannot be nested; declare them at top level");
        return parseDeclaration();
    }
    if (is("{")) return parseBlock();
    if (is(";")) { Token t = advance(); return mk(NK::Empty, "", t); }

    if (is("if")) {
        Token kw = advance();
        expect("(");
        auto n = mk(NK::IfStmt, "", kw);
        n->add(parseExpr());
        expectSoft(")");
        n->add(parseStatementSafe());
        if (match("else")) n->add(parseStatementSafe());   // else binds to nearest if
        return n;
    }
    if (is("while")) {                       // report, then parse it anyway so we resynchronize cleanly
        Token kw = advance();
        error(kw, "B-Minor has no 'while' statement (use 'for' instead)");
        auto n = mk(NK::Error, "while", kw);
        expect("(");
        n->add(parseExpr());
        expectSoft(")");
        n->add(parseStatementSafe());
        return n;
    }

    if (is("for")) {
        Token kw = advance();
        expect("(");
        auto n = mk(NK::ForStmt, "", kw);
        n->add(parseOptExpr(";"));  expect(";");
        n->add(parseOptExpr(";"));  expect(";");
        n->add(parseOptExpr(")"));  expectSoft(")");
        n->add(parseStatementSafe());
        return n;
    }
    if (is("return")) {
        Token kw = advance();
        auto n = mk(NK::ReturnStmt, "", kw);
        if (!is(";")) n->add(parseExpr());
        expectSoft(";");
        return n;
    }
    if (is("print")) {
        Token kw = advance();
        auto n = mk(NK::PrintStmt, "", kw);
        if (!is(";")) {
            do { n->add(parseExpr()); } while (match(","));
        }
        expectSoft(";");
        return n;
    }

    auto e = parseExpr();
    auto s = mkAt(NK::ExprStmt, "", *e);
    s->add(std::move(e));
    expectSoft(";");
    return s;
}

// ---------- expressions ----------
NodePtr Parser::parseExpr() { return parseAssignment(); }

NodePtr Parser::parseAssignment() {            // right-associative
    auto lhs = parseLevel(0);
    if (is("=")) {
        Token eq = advance();
        auto rhs = parseAssignment();
        if (lhs->kind != NK::Identifier && lhs->kind != NK::IndexExpr)
            error(eq, "invalid assignment target");
        auto n = mkAt(NK::BinaryExpr, "=", *lhs);
        n->add(std::move(lhs));
        n->add(std::move(rhs));
        return n;
    }
    return lhs;
}

NodePtr Parser::parseLevel(size_t level) {     // left-associative levels
    if (level == kLevels.size()) return parsePower();
    auto left = parseLevel(level + 1);
    while (true) {
        const Token& t = peek();
        bool hit = false;
        if (t.type == TokType::Operator)
            for (const auto& op : kLevels[level]) if (t.lexeme == op) hit = true;
        if (!hit) break;
        Token opTok = advance();
        auto right = parseLevel(level + 1);
        auto n = mkAt(NK::BinaryExpr, opTok.lexeme, *left);
        n->add(std::move(left));
        n->add(std::move(right));
        left = std::move(n);
    }
    return left;
}

NodePtr Parser::parsePower() {                 // right-associative: 2^3^2 = 2^(3^2)
    auto base = parseUnary();
    if (is("^")) {
        Token op = advance();
        auto exp = parsePower();
        auto n = mkAt(NK::BinaryExpr, op.lexeme, *base);
        n->add(std::move(base));
        n->add(std::move(exp));
        return n;
    }
    return base;
}

NodePtr Parser::parseUnary() {
    if (is("-") || is("!")) {
        Token op = advance();
        auto n = mk(NK::UnaryExpr, op.lexeme, op);
        n->add(parseUnary());
        return n;
    }
    return parsePostfix();
}

NodePtr Parser::parsePostfix() {
    auto e = parsePrimary();
    while (true) {
        if (is("[")) {
            advance();
            auto idx = parseExpr();
            expectSoft("]");
            auto n = mkAt(NK::IndexExpr, "", *e);
            n->add(std::move(e));
            n->add(std::move(idx));
            e = std::move(n);
        } else if (is("(")) {
            advance();
            auto call = mkAt(NK::CallExpr, e->kind == NK::Identifier ? e->text : "", *e);
            call->add(std::move(e));
            if (!is(")")) {
                do { call->add(parseExpr()); } while (match(","));
            }
            expectSoft(")");
            e = std::move(call);
        } else if (is("++") || is("--")) {
            Token op = advance();
            auto n = mkAt(NK::PostfixExpr, op.lexeme, *e);
            n->add(std::move(e));
            e = std::move(n);
        } else {
            break;
        }
    }
    return e;
}

NodePtr Parser::parsePrimary() {
    Token t = peek();
    switch (t.type) {
        case TokType::IntLit:    advance(); return mk(NK::IntLiteral, t.lexeme, t);
        case TokType::BoolLit:   advance(); return mk(NK::BoolLiteral, t.lexeme, t);
        case TokType::CharLit:   advance(); return mk(NK::CharLiteral, t.lexeme, t);
        case TokType::StringLit: advance(); return mk(NK::StringLiteral, t.lexeme, t);
        case TokType::Ident:     advance(); return mk(NK::Identifier, t.lexeme, t);
        default: break;
    }
    if (is("(")) {
        advance();
        auto e = parseExpr();
        expectSoft(")");
        return e;
    }
    fail("expected expression, got " + describe(t));
}
