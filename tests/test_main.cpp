#include <iostream>
#include <string>
#include <vector>
#include "AST.h"
#include "Highlighter.h"
#include "Lexer.h"
#include "Parser.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
                                            << ": " #c "\n"; ++failures; } } while (0)

static std::vector<Token> lex(const std::string& s, std::vector<Diagnostic>* errs = nullptr) {
    Lexer l(s, "t.bm");
    auto t = l.tokenize();
    if (errs) *errs = l.errors();
    return t;
}
static std::vector<Token> code(const std::vector<Token>& all) {
    std::vector<Token> r;
    for (auto& t : all) if (!t.isTrivia() && t.type != TokType::Eof) r.push_back(t);
    return r;
}
struct Parsed { NodePtr root; std::vector<Diagnostic> errs; std::vector<Token> toks; };
static Parsed parse(const std::string& s) {
    Parsed p;
    p.toks = lex(s);
    Parser ps(p.toks);
    p.root = ps.parseProgram();
    p.errs = ps.errors();
    return p;
}
static std::string stripAnsi(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b') { while (i < s.size() && s[i] != 'm') ++i; }
        else o += s[i];
    }
    return o;
}

int main() {
    // --- lexer ---
    { auto t = code(lex("a <= b == c"));
      CHECK(t.size() == 5 && t[1].lexeme == "<=" && t[3].lexeme == "=="); }          // maximal munch
    { auto t = code(lex("return returnValue"));
      CHECK(t[0].type == TokType::Keyword && t[1].type == TokType::Ident); }         // keyword priority
    { auto t = code(lex("true x"));
      CHECK(t[0].type == TokType::BoolLit && t[1].type == TokType::Ident); }
    { auto t = code(lex("n-1"));
      CHECK(t.size() == 3 && t[1].lexeme == "-" && t[2].type == TokType::IntLit); }  // no signed literals
    { auto t = code(lex("x\n  y"));
      CHECK(t[1].line == 2 && t[1].col == 3); }                                      // positions
    { std::vector<Diagnostic> e; auto t = code(lex("x: integer = 5@;", &e));
      CHECK(e.size() == 1 && e[0].col == 15);
      CHECK(t[4].type == TokType::IntLit && t[5].type == TokType::Invalid && t[6].lexeme == ";"); }
    { std::vector<Diagnostic> e; lex("s = \"abc", &e); CHECK(e.size() == 1); }       // unterminated string
    { std::vector<Diagnostic> e; lex("/* abc", &e);  CHECK(e.size() == 1); }         // unterminated comment
    { std::vector<Diagnostic> e; auto t = code(lex("\"a\\nb\" 'x' '\\n'", &e));
      CHECK(e.empty() && t[0].type == TokType::StringLit && t[1].type == TokType::CharLit &&
            t[2].type == TokType::CharLit); }
    { std::string src = "x: integer = 5@; // hi\n/* c */ \"s\\q\" 'ab' a & b\n";
      auto all = lex(src); std::string re; for (auto& t : all) re += t.lexeme;
      CHECK(re == src); }                                                            // lossless token stream

    // --- parser / AST ---
    { auto p = parse("f: function integer (n: integer) = { return n * f(n - 1); }");
      CHECK(p.errs.empty());
      std::string d = astToString(*p.root);
      CHECK(d.find("ReturnStmt") != std::string::npos);
      CHECK(d.find("CallExpr 'f'") != std::string::npos);
      CHECK(d.find("BinaryExpr '*'") != std::string::npos); }
    { auto p = parse("x: integer = 1 + 2 * 3;");
      const Node& init = *p.root->kids[0]->kids[1];
      CHECK(init.text == "+" && init.kids[1]->text == "*"); }                        // precedence
    { auto p = parse("x: integer = 2 ^ 3 ^ 2;");
      const Node& init = *p.root->kids[0]->kids[1];
      CHECK(init.text == "^" && init.kids[1]->text == "^"); }                        // right assoc
    { auto p = parse("x: integer = 10 - 3 - 2;");
      const Node& init = *p.root->kids[0]->kids[1];
      CHECK(init.text == "-" && init.kids[0]->text == "-"); }                        // left assoc
    { auto p = parse("x: integer = ;\ny: integer = 42;");
      CHECK(p.errs.size() == 1);
      CHECK(p.root->kids.size() == 2 && p.root->kids[1]->kind == NK::VarDecl &&
            p.root->kids[1]->text == "y"); }                                         // panic recovery
    { auto p = parse("f: function void () = { if (y > 0 { return; } }");
      CHECK(p.errs.size() == 1);
      CHECK(astToString(*p.root).find("IfStmt") != std::string::npos); }             // missing ')'
    { auto p = parse("a: array [3] integer = {1,2,3};\nf: function void (v: array [] integer);");
      CHECK(p.errs.empty()); }
    { auto p = parse("}}} x: integer = 1;"); CHECK(!p.errs.empty()); }               // must terminate
    { auto p = parse("f: function void () = { x = ; y = ; z = ; "); CHECK(!p.errs.empty()); }
    for (auto src : {"", "(((", ")", "x:", "x: function", "x: array [", "{ { {"}) {
        auto p = parse(src); CHECK(p.root != nullptr);                              // never crashes / loops
    }

    // --- highlighter ---
    { auto p = parse("f: function integer (n: integer) = { return g(n) + n; }");
      Highlighter h(*p.root);
      Cat cf = Cat::Plain, cg = Cat::Plain, cn = Cat::Plain, cint = Cat::Plain;
      for (auto& t : p.toks) {
          if (t.lexeme == "f") cf = h.classify(t);
          if (t.lexeme == "g") cg = h.classify(t);
          if (t.lexeme == "n") cn = h.classify(t);
          if (t.lexeme == "integer") cint = h.classify(t);
      }
      CHECK(cf == Cat::Func && cg == Cat::Func && cn == Cat::Var && cint == Cat::Type); }
    { std::string src = "x: integer = 5@; // c\ny: string = \"abc\n";
      auto p = parse(src); Highlighter h(*p.root);
      CHECK(stripAnsi(h.toAnsi(p.toks)) == src);
      std::string html = h.toHtml(p.toks, "t");
      CHECK(html.find("<pre") != std::string::npos && html.find("class=\"type\"") != std::string::npos); }

    // --- rules from the instructor's B-Minor slides ---
    { auto t = code(lex("while")); CHECK(t[0].type == TokType::Keyword); }           // while is reserved...
    { auto p = parse("f: function void () = { while (true) { } }");                  // ...but is not a statement
      CHECK(!p.errs.empty()); }
    { std::vector<Diagnostic> e; auto t = code(lex("'\\q' \"a\\qb\" '\\0' '\\''", &e));
      CHECK(e.empty() && t[0].type == TokType::CharLit && t[1].type == TokType::StringLit &&
            t[2].type == TokType::CharLit && t[3].type == TokType::CharLit); }       // backslash + any char
    { std::vector<Diagnostic> e; lex(std::string(257, 'a'), &e); CHECK(e.size() == 1); }
    { std::vector<Diagnostic> e; lex(std::string(256, 'a'), &e); CHECK(e.empty()); }
    { std::vector<Diagnostic> e; lex("\"" + std::string(257, 'a') + "\"", &e); CHECK(e.size() == 1); }
    { std::vector<Diagnostic> e; lex("\"" + std::string(256, 'a') + "\"", &e); CHECK(e.empty()); }
    { std::vector<Diagnostic> e; lex("9223372036854775808", &e); CHECK(e.empty()); }
    { std::vector<Diagnostic> e; lex("9223372036854775809", &e); CHECK(e.size() == 1); }
    { auto p = parse("f: function void () = { g: function void () = { } }");
      CHECK(p.errs.size() == 1); }                                                   // no nested functions
    { auto p = parse("x: boolean = a || b && c;");                                    // same level, left assoc
      const Node& init = *p.root->kids[0]->kids[1];
      CHECK(init.text == "&&" && init.kids[0]->text == "||"); }
    { auto p = parse("f: function void () = { if (y > 0 { return; } }");
      std::vector<Diagnostic> all = p.errs;
      Highlighter h(*p.root, all);
      std::string html = h.toHtml(p.toks, "t");
      CHECK(html.find("class=\"err\" title=\"expected ')' before '{'\"") != std::string::npos);
      CHECK(html.find("1 error(s)") != std::string::npos);
      CHECK(stripAnsi(h.toAnsi(p.toks)) == "f: function void () = { if (y > 0 { return; } }"); }

    if (failures == 0) std::cout << "all tests passed\n";
    return failures == 0 ? 0 : 1;
}
