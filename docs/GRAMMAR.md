# B-Minor grammar (EBNF) — LL, recursive descent

Terminals in quotes. `{ x }` = zero or more, `[ x ]` = optional, `|` = choice.

```
program        ::= { declaration } EOF
declaration    ::= IDENT ':' ( func_rest | var_rest )
func_rest      ::= 'function' type '(' [ param { ',' param } ] ')' ( ';' | '=' block )
var_rest       ::= type [ '=' ( array_init | expr ) ] ';'
param          ::= IDENT ':' type
type           ::= 'integer' | 'boolean' | 'char' | 'string' | 'void'
                 | 'array' '[' [ expr ] ']' type
array_init     ::= '{' [ expr { ',' expr } ] '}'

block          ::= '{' { statement } '}'
statement      ::= declaration                     (* IDENT ':' *)
                 | block
                 | 'if' '(' expr ')' statement [ 'else' statement ]
                 | 'for' '(' [expr] ';' [expr] ';' [expr] ')' statement
                 | 'return' [ expr ] ';'
                 | 'print' [ expr { ',' expr } ] ';'
                 | expr ';'
                 | ';'

expr           ::= assignment
assignment     ::= logic [ '=' assignment ]                 (* right assoc *)
logic          ::= comparison { ( '&&' | '||' ) comparison }    (* same row in the course table, left assoc *)
comparison     ::= additive { ( '<' | '<=' | '>' | '>=' | '==' | '!=' ) additive }
additive       ::= term { ( '+' | '-' ) term }
term           ::= power { ( '*' | '/' | '%' ) power }
power          ::= unary [ '^' power ]                          (* right assoc *)
unary          ::= ( '-' | '!' ) unary | postfix
postfix        ::= primary { '[' expr ']' | '(' [ expr { ',' expr } ] ')' | '++' | '--' }
primary        ::= INT | BOOL | CHAR | STRING | IDENT | '(' expr ')'
```

Examples of accepted declarations:
`x: integer = 5;`  `a: array [3] integer = {1,2,3};`
`f: function integer (n: integer);`  `f: function integer (n: integer) = { ... }`

## Why this grammar suits recursive descent

* **No left recursion.** Textbook form `additive ::= additive '+' term | term` is left-recursive;
  it was rewritten as `additive ::= term { ('+'|'-') term }` (the standard
  `A -> A a | b  ==>  A -> b {a}` transformation) and the tree is built left-associatively in the loop.
* **Precedence** is encoded by the chain of rules (lowest first): `=`, `&& ||` (one level, as in the course table), comparison,
  `+ -`, `* / %`, `^`, unary, postfix. `^` is right-associative; unary binds tighter than `^`
  (so `-2^2` = `(-2)^2`), exactly as the instructor's operator table lists `- !` above `^`.
* **FIRST/FOLLOW checks (key points)**
  * `declaration`: FIRST = {IDENT}; a statement starting with IDENT is a declaration iff the next
    token is `:` — this is the single LL(2) decision (`expr_stmt` can never contain `IDENT ':'`).
  * `var_rest` initializer: FIRST(array_init) = {`{`} is disjoint from FIRST(expr) = {INT, BOOL, CHAR, STRING, IDENT, `(`, `-`, `!`}.
  * `if ... [else statement]`: classic dangling else; resolved greedily (else binds to the nearest `if`),
    which is what the `if(match("else"))` test does. FIRST(else) = {else} ∈ FOLLOW(statement), the usual benign conflict.
  * `{ op operand }` loops: continue iff the next token is in the operator set of that level, which is
    disjoint from FOLLOW(expr) = {`;` `)` `]` `,` `}` ...}.
  * `type` alternatives start with distinct keywords.

## Language rules from the course slides that affect the grammar

* No `while`, `do-while` or `switch` statements (`while` stays a reserved word). Using it gives a clear error.
* Functions cannot be nested: a `function` declaration inside a block is reported.
* Statements allowed in a function body: declarations (with initializers), expression, `return`, `print`, `if`/`else`
  (so `else if` works), `for`, and blocks.
* Function parameters are `name : type`; array parameters may omit the size: `a: array [] integer`.
* Not checked by the parser (Phase 2, semantics): global initializers must be constants, array sizes constant,
  strict typing, `main` must exist and return integer.
* Note: slide 14 writes the prototype `writechar: function void ( char c );`; the book and every other slide use
  `( c: char )`, which is what this grammar accepts.

## FIRST / FOLLOW (the sets that matter for the LL decisions)

| Non-terminal | FIRST | FOLLOW |
|---|---|---|
| `declaration` | IDENT | IDENT, `{`, `;`, `if`, `for`, `return`, `print`, literals, `(`, `-`, `!`, `}`, EOF |
| `type` | integer boolean char string void array | `=` `;` `)` `,` `(` |
| `expr` | INT BOOL CHAR STRING IDENT `(` `-` `!` | `;` `)` `]` `,` `}` |
| `array_init` | `{` | `;` |
| `statement` | IDENT `{` `;` `if` `for` `return` `print` INT BOOL CHAR STRING `(` `-` `!` | `else` `}` and FIRST(statement) |
| `param` | IDENT | `,` `)` |

Conflict check: inside `statement`, IDENT starts both a declaration and an expression -> decided with one extra token
(`IDENT ':'`), i.e. LL(2) at that single point. FIRST(`array_init`) = {`{`} is disjoint from FIRST(`expr`).
The only remaining overlap, `else` in FOLLOW(statement), is the dangling-else case (resolved greedily).

## AST shape

See the comment above `struct Node` in `src/AST.h`. Every node stores `line:col` of its first token and
an empty `typeInfo` field reserved for Phase 2 (semantic analysis).

## Error recovery

* Panic mode: on a hard error (`ParseError`) the parser skips tokens until `;` (consumed), `}`,
  a statement keyword (`if for return print`); a skipped `{ ... }` group is skipped as a whole or `IDENT ':'` (start of a declaration),
  then continues. An `ErrorNode` is placed in the tree.
* Soft expectations for `)`, `]`, `;`: report "expected ')' before '{'" but do not skip anything —
  the construct is parsed as if the token were present.
* Progress guarantee: if synchronize() did not move, one token is skipped, so the parser always terminates.
