//===--- ASTDumper.cpp ------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/ASTVisitor.hpp"
#include "Sora/AST/Decl.hpp"
#include "Sora/AST/Expr.hpp"
#include "Sora/AST/Pattern.hpp"
#include "Sora/AST/Stmt.hpp"
#include "Sora/AST/TypeRepr.hpp"
#include "Sora/Common/LLVM.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "llvm/Support/raw_ostream.h"

using namespace sora;

namespace {

const char *getKindStr(DeclKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("unknown DeclKind");
#define DECL(ID, PARENT)                                                       \
  case DeclKind::ID:                                                           \
    return #ID "Decl";
#include "Sora/AST/DeclNodes.def"
  }
}

const char *getKindStr(ExprKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("unknown ExprKind");
#define EXPR(ID, PARENT)                                                       \
  case ExprKind::ID:                                                           \
    return #ID "Expr";
#include "Sora/AST/ExprNodes.def"
  }
}

const char *getKindStr(StmtKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("unknown StmtKind");
#define STMT(ID, PARENT)                                                       \
  case StmtKind::ID:                                                           \
    return #ID "Stmt";
#include "Sora/AST/StmtNodes.def"
  }
}

const char *getKindStr(PatternKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("unknown PatternKind");
#define PATTERN(ID, PARENT)                                                    \
  case PatternKind::ID:                                                        \
    return #ID "Pattern";
#include "Sora/AST/PatternNodes.def"
  }
}

const char *getKindStr(TypeReprKind kind) {
  switch (kind) {
  default:
    llvm_unreachable("unknown TypeReprKind");
#define TYPEREPR(ID, PARENT)                                                   \
  case TypeReprKind::ID:                                                       \
    return #ID "TypeRepr";
#include "Sora/AST/TypeReprNodes.def"
  }
}

class Dumper : public SimpleASTVisitor<Dumper> {
  raw_ostream &out;
  const SourceManager &srcMgr;
  unsigned curIndent;
  const unsigned indentSize;

  /// RAII class that increases/decreases indentation when it's
  /// constructed/destroyed.
  struct IncreaseIndentRAII {
    Dumper &dumper;

    IncreaseIndentRAII(Dumper &dumper) : dumper(dumper) {
      dumper.curIndent += dumper.indentSize;
    }

    ~IncreaseIndentRAII() { dumper.curIndent -= dumper.indentSize; }
  };

  /// Increases the indentation temporarily (until the returned object is
  /// destroyed)
  IncreaseIndentRAII increaseIndent() { return IncreaseIndentRAII(*this); }

  /// PrintBase methods indent the output stream and print basic information
  /// about a class.

  void printCommon(Decl *decl) {
    out.indent(curIndent);
    out << getKindStr(decl->getKind());
  }

  void printCommon(Expr *expr) {
    out.indent(curIndent);
    out << getKindStr(expr->getKind());
  }

  void printCommon(Stmt *stmt) {
    out.indent(curIndent);
    out << getKindStr(stmt->getKind());
  }

  void printCommon(TypeRepr *tyRepr) {
    out.indent(curIndent);
    out << getKindStr(tyRepr->getKind());
  }

  void printCommon(Pattern *pattern) {
    out.indent(curIndent);
    out << getKindStr(pattern->getKind());
  }

  /// For null nodes.
  void printNoNode() {
    out.indent(curIndent);
    out << "<no node>\n";
  }

  void printLoc(SourceLoc loc, StringRef name) {
    out << name << '=';
    loc.print(out, srcMgr, false);
  }

  void printRange(SourceRange range, StringRef name) {
    out << name << '=';
    range.print(out, srcMgr, false);
  }

public:
  Dumper(raw_ostream &out, const SourceManager &srcMgr, unsigned indentSize)
      : out(out), curIndent(0), indentSize(indentSize), srcMgr(srcMgr) {}

  /// Override the visit method so it calls printNoNode() when the node is null.
  template <typename T> void visit(T node) {
    node ? ASTVisitor::visit(node) : printNoNode();
  }

  /// Provide an alternative visit entry point that only visits the node if
  /// it's non-null, else it ignores it.
  template <typename T> void visitIf(T node) {
    if (node)
      ASTVisitor::visit(node);
  }

  //===--- Stmt -----------------------------------------------------------===//

  void visitContinueStmt(ContinueStmt *stmt) {
    printCommon(stmt);
    out << ' ';
    printLoc(stmt->getLoc(), "loc");
  }

  void visitBreakStmt(BreakStmt *stmt) {
    printCommon(stmt);
    out << ' ';
    printLoc(stmt->getLoc(), "loc");
  }

  void visitReturnStmt(ReturnStmt *stmt) {
    printCommon(stmt);
    out << ' ';
    printLoc(stmt->getLoc(), "loc");
    out << '\n';
    if (!stmt->hasResult())
      return;

    if (stmt->hasResult()) {
      auto indent = increaseIndent();
      visit(stmt->getResult());
    }
  }

  void visitBlockStmt(BlockStmt *stmt) {
    printCommon(stmt);
    out << " numElement=" << stmt->getNumElements();
    out << ' ';
    printLoc(stmt->getLeftCurlyLoc(), "leftCurly");
    out << ' ';
    printLoc(stmt->getRightCurlyLoc(), "rightCurly");
    for (auto elem : stmt->getElements())
      visit(elem);
  }

  void visitIfStmt(IfStmt *stmt) {
    // Print node
    printCommon(stmt);
    out << ' ';
    printLoc(stmt->getIfLoc(), "ifLoc");
    if (SourceLoc loc = stmt->getElseLoc())
      printLoc(loc, "elseLoc");
    // Print children
    auto indent = increaseIndent();
    visit(stmt->getThen());
    visitIf(stmt->getElse());
  }

  void visitWhileStmt(WhileStmt *stmt) {
    // Print node
    printCommon(stmt);
    out << ' ';
    printLoc(stmt->getWhileLoc(), "whileLoc");
    // Print children
    auto indent = increaseIndent();
    visit(stmt->getBody());
  }

  //===--- Expr -----------------------------------------------------------===//

  // TODO
  void visitExpr(Expr *expr) {
    printCommon(expr);
    out << " TODO\n";
  }

  //===--- Decl -----------------------------------------------------------===//

  // TODO
  void visitDecl(Decl *decl) {
    printCommon(decl);
    out << " TODO\n";
  }

  //===--- Pattern --------------------------------------------------------===//

  // TODO
  void visitPattern(Pattern *pattern) {
    printCommon(pattern);
    out << " TODO\n";
  }

  //===--- TypeRepr -------------------------------------------------------===//

  // TODO
  void visitTypeRepr(TypeRepr *tyRepr) {
    printCommon(tyRepr);
    out << " TODO\n";
  }
};
} // namespace

void Decl::dump(raw_ostream &out, const SourceManager &srcMgr,
                unsigned indent) {
  Dumper(out, srcMgr, indent).visit(this);
}

void Expr::dump(raw_ostream &out, const SourceManager &srcMgr,
                unsigned indent) {
  Dumper(out, srcMgr, indent).visit(this);
}

void Pattern::dump(raw_ostream &out, const SourceManager &srcMgr,
                   unsigned indent) {
  Dumper(out, srcMgr, indent).visit(this);
}

void TypeRepr::dump(raw_ostream &out, const SourceManager &srcMgr,
                    unsigned indent) {
  Dumper(out, srcMgr, indent).visit(this);
}

void Stmt::dump(raw_ostream &out, const SourceManager &srcMgr,
                unsigned indent) {
  Dumper(out, srcMgr, indent).visit(this);
}