#pragma once
#include <memory>
#include <ostream>
#include <string>
#include <vector>

enum class NK {
    Program, VarDecl, FuncDecl, Param, TypeName, ArrayType,
    Block, IfStmt, ForStmt, ReturnStmt, PrintStmt, ExprStmt, Empty, Error,
    BinaryExpr, UnaryExpr, PostfixExpr, CallExpr, IndexExpr, ArrayInit,
    Identifier, IntLiteral, BoolLiteral, CharLiteral, StringLiteral
};

const char* nodeKindName(NK k);

struct Node;
using NodePtr = std::unique_ptr<Node>;

// Child layout per kind (documented in docs/GRAMMAR.md):
//  Program    : decls...
//  VarDecl    : [type, init?]            text = name, loc = name token
//  FuncDecl   : [retType, Param*, Block?] text = name, loc = name token
//  Param      : [type]                   text = name
//  ArrayType  : [elemType, sizeExpr?]
//  IfStmt     : [cond, then, else?]      ForStmt: [init, cond, next, body] (Empty if omitted)
//  CallExpr   : [callee, args...]        text = callee name if it is an identifier
//  BinaryExpr : [lhs, rhs]               text = operator ("=" is assignment)
struct Node {
    NK kind;
    std::string text;
    int line, col;                 // first token of the node
    std::string typeInfo;          // type annotation: empty now, filled in Phase 2
    std::vector<NodePtr> kids;

    Node(NK k, std::string t, int l, int c)
        : kind(k), text(std::move(t)), line(l), col(c) {}

    Node* add(NodePtr child) {
        kids.push_back(std::move(child));
        return kids.back().get();
    }
};

void dumpAst(const Node& n, std::ostream& os, int depth = 0);
std::string astToString(const Node& n);
