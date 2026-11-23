# B-Minor lexical specification (Phase 1)

Each token class is a regular language. The scanner in `src/Lexer.cpp` is a hand-written
DFA: every `scanXxx()` function is the code form of one automaton (state = position in the
function, transition = `peek()` test).

| Class | Regular expression | Notes |
|---|---|---|
| Whitespace | `[ \t\r\n]+` | kept as trivia (needed for exact output), never given to the parser |
| Line comment | `//[^\n]*` | ends before `\n` |
| Block comment | `/\*([^*]\|\*+[^*/])*\*+/` | no nesting; unclosed -> INVALID + error |
| Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` | max 256 characters (longer -> error); looked up in the keyword table after matching |
| Keyword | `array boolean char else false for function if integer print return string true void while` | finite set from the course slides; `true`/`false` are reported as BOOL literals; `while` is reserved although the language has no `while` statement |
| Boolean literal | `true \| false` | |
| Integer literal | `[0-9]+` | 64-bit signed range (magnitude up to 2^63, error if larger). **unsigned**: `-` is an operator (otherwise `n-1` would lex as `n` `-1`); unary minus is parsed by the parser |
| String literal | `"([^"\\\n]\|\\[^\n])*"` | cannot span lines; max 256 chars after escapes; `\n` newline, `\0` NUL, backslash + any other char = that char |
| Char literal | `'([^'\\\n]\|\\[^\n])'` | exactly one ASCII character or escape |
| Operator | `+ - * / % ^ ! < > = == != <= >= && \|\| ++ --` | |
| Delimiter | `( ) [ ] { } , : ;` | |

## Which rule wins? (conflict resolution)

1. **Longest match (maximal munch)**: the scanner always consumes as many characters as the
   current automaton allows. `<=` is one token (two-character operators are tried before
   one-character ones), `==` is not `=` `=`, `x1` is one identifier, `//` starts a comment
   rather than being two `/`.
2. **Priority**: if two rules match the *same* length, keyword beats identifier. Implementation:
   the identifier automaton runs first, then the lexeme is looked up in the keyword table, so
   `return` -> KEYWORD but `returnValue` -> IDENT (longest match already made it a longer ident).

## Error handling

* unknown character (`@`, `#`, lone `&`, lone `|`, non-ASCII byte...) -> one INVALID token,
  error with line:col, scanning continues after it
* unterminated string / invalid escape / bad char literal / unterminated block comment ->
  INVALID token covering the bad text + error, scanning continues
* the lexer never throws and never stops; the token stream always ends with EOF

Theory link: regex -> Thompson NFA -> subset construction DFA -> (Hopcroft) minimal DFA is the
generic pipeline used by lex/flex; here the DFAs were written directly in code.
