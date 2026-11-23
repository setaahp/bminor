// B-Minor tool, phase 1:  source -> Lexer -> Parser(AST) -> Highlighter
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include "Highlighter.h"
#include "Lexer.h"
#include "Parser.h"
#ifdef _WIN32
#include <windows.h>
#endif

static std::string escapeForPrint(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\r') o += "\\r";
        else o += c;
    }
    return o;
}

static void usage() {
    std::cerr << "usage: bminor <file.bm> [--tokens] [--ast] [--ansi] [--html out.html]\n"
                 "  (no option) = --ansi: colourised source on stdout; errors on stderr\n";
}

// Windows consoles need ANSI escape processing switched on explicitly.
static void enableAnsiOnWindows() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004);   // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
}

int main(int argc, char** argv) {
    enableAnsiOnWindows();
    if (argc < 2) { usage(); return 2; }
    std::string path = argv[1];
    bool tokens = false, ast = false, ansi = false;
    std::string htmlOut;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tokens") tokens = true;
        else if (a == "--ast") ast = true;
        else if (a == "--ansi") ansi = true;
        else if (a == "--html" && i + 1 < argc) htmlOut = argv[++i];
        else { usage(); return 2; }
    }
    if (!tokens && !ast && htmlOut.empty()) ansi = true;

    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "cannot open " << path << "\n"; return 2; }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();
    if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) source.erase(0, 3);   // skip UTF-8 BOM (Windows editors)

    Lexer lexer(source, path);
    std::vector<Token> all = lexer.tokenize();
    Parser parser(all);
    NodePtr root = parser.parseProgram();

    std::vector<Diagnostic> diags = lexer.errors();
    diags.insert(diags.end(), parser.errors().begin(), parser.errors().end());
    std::stable_sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return a.line != b.line ? a.line < b.line : a.col < b.col;
    });
    for (const auto& d : diags) std::cerr << d.str() << "\n";

    if (tokens) {
        for (const auto& t : all) {
            if (t.isTrivia()) continue;
            std::cout << t.line << ":" << t.col << "\t" << tokTypeName(t.type) << "\t'"
                      << escapeForPrint(t.lexeme) << "'\n";
        }
    }
    if (ast) dumpAst(*root, std::cout);

    Highlighter hl(*root, diags);
    if (ansi) std::cout << hl.toAnsi(all);
    if (!htmlOut.empty()) {
        std::ofstream out(htmlOut, std::ios::binary);
        out << hl.toHtml(all, path);
    }
    return diags.empty() ? 0 : 1;
}
