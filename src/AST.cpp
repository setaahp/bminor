#include "AST.h"
#include <sstream>

const char* nodeKindName(NK k) {
    switch (k) {
        case NK::Program:       return "Program";
        case NK::VarDecl:       return "VarDecl";
        case NK::FuncDecl:      return "FuncDecl";
        case NK::Param:         return "Param";
        case NK::TypeName:      return "TypeName";
        case NK::ArrayType:     return "ArrayType";
        case NK::Block:         return "Block";
        case NK::IfStmt:        return "IfStmt";
        case NK::ForStmt:       return "ForStmt";
        case NK::ReturnStmt:    return "ReturnStmt";
        case NK::PrintStmt:     return "PrintStmt";
        case NK::ExprStmt:      return "ExprStmt";
        case NK::Empty:         return "Empty";
        case NK::Error:         return "ErrorNode";
        case NK::BinaryExpr:    return "BinaryExpr";
        case NK::UnaryExpr:     return "UnaryExpr";
        case NK::PostfixExpr:   return "PostfixExpr";
        case NK::CallExpr:      return "CallExpr";
        case NK::IndexExpr:     return "IndexExpr";
        case NK::ArrayInit:     return "ArrayInit";
        case NK::Identifier:    return "Identifier";
        case NK::IntLiteral:    return "IntLiteral";
        case NK::BoolLiteral:   return "BoolLiteral";
        case NK::CharLiteral:   return "CharLiteral";
        case NK::StringLiteral: return "StringLiteral";
    }
    return "?";
}

void dumpAst(const Node& n, std::ostream& os, int depth) {
    os << std::string(depth * 2, ' ') << nodeKindName(n.kind);
    if (!n.text.empty()) os << " '" << n.text << "'";
    os << " (" << n.line << ":" << n.col << ")";
    if (!n.typeInfo.empty()) os << " : " << n.typeInfo;
    os << "\n";
    for (const auto& k : n.kids) dumpAst(*k, os, depth + 1);
}

std::string astToString(const Node& n) {
    std::ostringstream os;
    dumpAst(n, os);
    return os.str();
}
