#include "Highlighter.h"
#include <algorithm>

namespace {

const char* ansiStart(Cat c) {
    switch (c) {
        case Cat::Keyword:  return "\x1b[1;34m";        // bold blue
        case Cat::Type:     return "\x1b[36m";          // cyan
        case Cat::Func:     return "\x1b[33m";          // yellow
        case Cat::Number:   return "\x1b[38;5;208m";    // orange
        case Cat::String:   return "\x1b[32m";          // green
        case Cat::Operator: return "\x1b[37m";          // light gray
        case Cat::Comment:  return "\x1b[3;90m";        // dim gray, italic
        case Cat::Error:    return "\x1b[31;4m";        // red underline
        default:            return "";                  // Var / Plain: default colour
    }
}

const char* htmlClass(Cat c) {
    switch (c) {
        case Cat::Keyword:  return "kw";
        case Cat::Type:     return "type";
        case Cat::Func:     return "fn";
        case Cat::Var:      return "var";
        case Cat::Number:   return "lit num";
        case Cat::String:   return "lit str";
        case Cat::Operator: return "op";
        case Cat::Comment:  return "cm";
        case Cat::Error:    return "err";
        default:            return "";
    }
}

std::string htmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default:  o += c;
        }
    }
    return o;
}

}  // namespace

Highlighter::Highlighter(const Node& astRoot, std::vector<Diagnostic> diags)
    : diags_(std::move(diags)) {
    collect(astRoot);
    std::stable_sort(diags_.begin(), diags_.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return a.line != b.line ? a.line < b.line : a.col < b.col;
    });
    for (const auto& d : diags_) {
        std::string& m = errAt_[{d.line, d.col}];
        if (!m.empty()) m += "\n";
        m += d.message;
    }
}

void Highlighter::collect(const Node& n) {
    const auto here = std::make_pair(n.line, n.col);
    switch (n.kind) {
        case NK::FuncDecl: roles_[here] = Cat::Func; break;     // declared function name
        case NK::VarDecl:
        case NK::Param:    roles_[here] = Cat::Var; break;      // declared variable name
        case NK::TypeName: roles_[here] = Cat::Type; break;     // integer, boolean, ...
        case NK::CallExpr:                                      // callee of a call = function
            if (!n.kids.empty() && n.kids[0]->kind == NK::Identifier)
                roles_[{n.kids[0]->line, n.kids[0]->col}] = Cat::Func;
            break;
        case NK::Identifier: roles_.emplace(here, Cat::Var); break;   // never overrides Func
        default: break;
    }
    for (const auto& k : n.kids) collect(*k);
}

Cat Highlighter::classify(const Token& t) const {
    auto role = roles_.find({t.line, t.col});
    switch (t.type) {
        case TokType::Keyword:
            return (role != roles_.end() && role->second == Cat::Type) ? Cat::Type : Cat::Keyword;
        case TokType::Ident:
            return role != roles_.end() ? role->second : Cat::Var;
        case TokType::IntLit:
        case TokType::BoolLit:      return Cat::Number;
        case TokType::StringLit:
        case TokType::CharLit:      return Cat::String;
        case TokType::Operator:     return Cat::Operator;
        case TokType::LineComment:
        case TokType::BlockComment: return Cat::Comment;
        case TokType::Invalid:      return Cat::Error;
        default:                    return Cat::Plain;
    }
}

std::string Highlighter::toAnsi(const std::vector<Token>& tokens) const {
    std::string out;
    for (const auto& t : tokens) {
        bool bad = !t.isTrivia() && errAt_.count({t.line, t.col}) > 0;
        if (t.type == TokType::Eof) {
            if (bad) out += std::string(ansiStart(Cat::Error)) + "<EOF>" + "\x1b[0m";
            continue;
        }
        const char* start = ansiStart(bad ? Cat::Error : classify(t));
        if (*start == '\0' || t.lexeme.empty()) out += t.lexeme;
        else out += std::string(start) + t.lexeme + "\x1b[0m";
    }
    return out;
}

std::string Highlighter::toHtml(const std::vector<Token>& tokens, const std::string& title) const {
    std::string out =
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>" +
        htmlEscape(title) + "</title>\n<style>\n"
        "body { background:#1e1e1e; margin:0; padding:1.5rem; color:#d4d4d4; font-family:system-ui,sans-serif; }\n"
        ".wrap { display:flex; font:14px/1.5 Consolas, Menlo, monospace; }\n"
        "pre { margin:0; tab-size:4; }\n"
        "pre.gutter { color:#6b6b6b; text-align:right; padding-right:1rem; user-select:none; }\n"
        "pre.code { color:#d4d4d4; }\n"
        ".kw   { color:#569cd6; font-weight:bold; }\n"
        ".type { color:#4ec9b0; }\n"
        ".fn   { color:#e5c07b; }\n"
        ".var  { color:#d4d4d4; }\n"
        ".lit.num { color:#d19a66; }\n"
        ".lit.str { color:#98c379; }\n"
        ".op   { color:#b0b0b0; }\n"
        ".cm   { color:#7f848e; font-style:italic; }\n"
        ".err  { color:#ff5555; text-decoration:underline wavy #ff5555; cursor:help; }\n"
        ".diag { margin-top:1.5rem; padding:.75rem 1rem; border-left:4px solid #ff5555; background:#2a2020; font:13px/1.6 Consolas, Menlo, monospace; }\n"
        ".diag.ok { border-color:#4ec9b0; background:#1f2a28; }\n"
        ".diag h2 { margin:0 0 .4rem; font-size:14px; }\n"
        ".diag ul { margin:0; padding-left:1.2rem; }\n"
        ".pos { color:#e5c07b; } .ph { color:#7f848e; }\n"
        "</style>\n</head>\n<body>\n";

    std::string code;
    int lines = 1;
    for (const auto& t : tokens) {
        for (char c : t.lexeme) if (c == '\n') ++lines;
        auto err = t.isTrivia() ? errAt_.end() : errAt_.find({t.line, t.col});
        bool bad = err != errAt_.end();
        std::string text = t.type == TokType::Eof ? (bad ? "\xE2\x9F\xA8" "EOF" "\xE2\x9F\xA9" : "") : t.lexeme;
        if (text.empty()) continue;
        if (bad) {
            std::string tip = htmlEscape(err->second);
            for (size_t i = 0; (i = tip.find('\n', i)) != std::string::npos; i += 5) tip.replace(i, 1, "&#10;");
            code += "<span class=\"err\" title=\"" + tip + "\">" + htmlEscape(text) + "</span>";
            continue;
        }
        const char* cls = htmlClass(classify(t));
        if (*cls == '\0') code += htmlEscape(text);
        else code += std::string("<span class=\"") + cls + "\">" + htmlEscape(text) + "</span>";
    }
    std::string gutter;
    for (int i = 1; i <= lines; ++i) gutter += std::to_string(i) + "\n";

    out += "<div class=\"wrap\"><pre class=\"gutter\">" + gutter + "</pre><pre class=\"code\">" + code +
           "</pre></div>\n";
    if (diags_.empty()) {
        out += "<div class=\"diag ok\"><h2>No errors</h2></div>\n";
    } else {
        out += "<div class=\"diag\"><h2>" + std::to_string(diags_.size()) + " error(s)</h2><ul>\n";
        for (const auto& d : diags_)
            out += "<li><span class=\"pos\">" + std::to_string(d.line) + ":" + std::to_string(d.col) +
                   "</span> <span class=\"ph\">" + htmlEscape(d.phase) + "</span> " + htmlEscape(d.message) + "</li>\n";
        out += "</ul></div>\n";
    }
    out += "</body>\n</html>\n";
    return out;
}
