# B-Minor — Phase 1 (lexer, parser, AST, syntax highlighter)

Build: `cmake -S . -B build && cmake --build build`  (or `g++ -std=c++17 -Isrc src/*.cpp -o bminor`)
Run:   `bminor tests/samples/factorial.bm`                 (ANSI colours in the terminal)
       `bminor file.bm --tokens | --ast | --html out.html`
Test:  `ctest --test-dir build` or `./build/bminor_tests`

Layout: `src/Token.*` `src/Lexer.*` `src/AST.*` `src/Parser.*` `src/Highlighter.*` `src/main.cpp`,
`docs/LEXICAL.md`, `docs/GRAMMAR.md`, `tests/`.

Pipeline: source -> Lexer (all tokens incl. trivia) -> Parser (non-trivia tokens -> AST)
-> Highlighter (AST roles + full token stream -> ANSI / HTML).

Errors: lexical and syntax errors go to stderr as `file:line:col: kind error: message`.
In `--ansi` and `--html` output the offending token is underlined in red; in HTML hovering shows the message
and a "diagnostics" panel with line:col is added under the code.
Language rules follow the instructor's B-Minor slides (see docs/GRAMMAR.md, "Language rules").
