//===--- Stmt.hpp - Statement ASTs -----------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/ASTAlignement.hpp"
#include "Sora/AST/ASTNode.hpp"
#include "Sora/Common/LLVM.hpp"
#include "Sora/Common/SourceLoc.hpp"
#include "llvm/Support/TrailingObjects.h"
#include <cassert>
#include <stdint.h>

namespace sora {
class Expr;
class ASTContext;

/// Kinds of Statements
enum class StmtKind : uint8_t {
#define STMT(KIND, PARENT) KIND,
#define STMT_RANGE(KIND, FIRST, LAST) First_##KIND = FIRST, Last_##KIND = LAST,
#include "Sora/AST/StmtNodes.def"
};

/// Base class for every Statement node.
class alignas(StmtAlignement) Stmt {
  // Disable vanilla new/delete for statements
  void *operator new(size_t) noexcept = delete;
  void operator delete(void *)noexcept = delete;

  StmtKind kind;

protected:
  // Children should be able to use placement new, as it is needed for children
  // with trailing objects.
  void *operator new(size_t, void *mem) noexcept {
    assert(mem);
    return mem;
  }

  Stmt(StmtKind kind) : kind(kind) {}

public:
  // Publicly allow allocation of statements using the ASTContext.
  void *operator new(size_t size, ASTContext &ctxt,
                     unsigned align = alignof(Stmt));

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const;
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const;
  /// \returns the full range of this statement
  SourceRange getSourceRange() const;

  /// \return the kind of statement this is
  StmtKind getKind() const { return kind; }
};

/// Represents a simple "break" statement indicating that we can break out of
/// the innermost loop.
class BreakStmt final : public Stmt {
  SourceLoc loc;

public:
  /// \param loc the SourceLoc of the "break" keyword
  BreakStmt(SourceLoc loc) : Stmt(StmtKind::Break), loc(loc) {}

  /// \returns the SourceLoc of the "break" keyword
  SourceLoc getLoc() const { return loc; }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const { return loc; }
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const { return loc; }

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::Break;
  }
};

/// Represents a simple "continue" statement, indicating that
/// we can skip the rest of the innermost loop and move on to the next
/// iteration (re-evaluating the condition first).
class ContinueStmt final : public Stmt {
  SourceLoc loc;

public:
  ContinueStmt(SourceLoc loc) : Stmt(StmtKind::Continue), loc(loc) {}

  SourceLoc getLoc() const { return loc; }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const { return loc; }
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const { return loc; }

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::Continue;
  }
};

/// Represents a "return" statement
///
/// \verbatim
///   return
///   return 0
/// \endverbatim
class ReturnStmt : public Stmt {
  SourceLoc returnLoc;
  Expr *result = nullptr;

public:
  ReturnStmt(SourceLoc returnLoc, Expr *result = nullptr)
      : Stmt(StmtKind::Return), returnLoc(returnLoc), result(result) {}

  SourceLoc getReturnLoc() const { return returnLoc; }

  bool hasResult() const { return (bool)result; }
  Expr *getResult() const { return result; }
  void setResult(Expr *expr) { this->result = expr; }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const;
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const;

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::Return;
  }
};

/// Represents a "Block" statement, which is a group of statements (ast nodes)
/// enclosed in curly brackets.
///
/// \verbatim
/// {
///   let x = 3*3
///   x += 2
/// }
/// \endverbatim
class BlockStmt final : public Stmt,
                        private llvm::TrailingObjects<BlockStmt, ASTNode> {
  friend llvm::TrailingObjects<BlockStmt, ASTNode>;
  size_t numTrailingObjects(OverloadToken<ASTNode>) { return numElem; }

  SourceLoc lCurlyLoc, rCurlyLoc;
  size_t numElem = 0;

  BlockStmt(SourceLoc lCurlyLoc, ArrayRef<ASTNode> nodes, SourceLoc rCurlyLoc);

public:
  /// Creates a Block Stmt
  static BlockStmt *create(ASTContext &ctxt, SourceLoc lCurlyLoc,
                           ArrayRef<ASTNode> nodes, SourceLoc rCurlyLoc);

  /// Creates an empty Block Stmt
  static BlockStmt *createEmpty(ASTContext &ctxt, SourceLoc lCurlyLoc,
                                SourceLoc rCurlyLoc);

  SourceLoc getLeftCurlyLoc() const { return lCurlyLoc; }
  SourceLoc getRightCurlyLoc() const { return rCurlyLoc; }

  size_t getNumElements() const { return numElem; }
  ArrayRef<ASTNode> getElements() const;
  MutableArrayRef<ASTNode> getElements();
  ASTNode getElement(size_t n) const;
  void setElement(size_t n, ASTNode node);

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const { return lCurlyLoc; }
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const { return rCurlyLoc; }

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::Block;
  }
};

/// Represents the condition of a loop or an if statement.
/// This can be a boolean expression (or a "let" declaration in the future)
class StmtCondition final {
  Expr *expr = nullptr; // FIXME: Make this a PointerUnion w/ a "let" decl

public:
  /// Creates a null (empty) condition
  StmtCondition() = default;

  /// Creates an expression condition
  StmtCondition(Expr *expr) : expr(expr) {}

  bool isExpr() const { return true; }
  Expr *getExpr() const {
    assert(isExpr() && "not an expr");
    return expr;
  }
  void setExpr(Expr *expr) {
    assert(isExpr() && "not an expr");
    this->expr = expr;
  }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const;
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const;
};

/// Common base class for conditional statements ("if-then-else" & "while")
class ConditionalStmt : public Stmt {
  StmtCondition cond;

protected:
  ConditionalStmt(StmtKind kind, StmtCondition cond) : Stmt(kind), cond(cond) {}

public:
  StmtCondition getCond() const { return cond; }
  void setCond(StmtCondition cond) { this->cond = cond; }

  static bool classof(const Stmt *stmt) {
    return (stmt->getKind() >= StmtKind::First_Conditional) &&
           (stmt->getKind() <= StmtKind::Last_Conditional);
  }
};

/// Represents an "if-then-else" statement
///
/// \verbatim
/// if cond {
///   /* do something */
/// }
///
/// if cond {
///   /* do something */
/// }
/// else {
///   /* do something */
/// }
///
/// if cond {
///   /* do something */
/// }
/// else if cond {
///   /* do something */
/// }
///
/// /*etc*/
/// \endverbatim
class IfStmt final : public ConditionalStmt {
  Stmt *thenStmt = nullptr;
  Stmt *elseStmt = nullptr;
  SourceLoc ifLoc;
  SourceLoc elseLoc;

public:
  /// \param ifLoc the SourceLoc of the "if" keyword
  /// \param cond the condition
  /// \param thenStmt the "then" statement
  /// \param ifLoc the SourceLoc of the "else" keyword
  /// \param thenStmt the "else" statement
  IfStmt(SourceLoc ifLoc, StmtCondition cond, Stmt *thenStmt, SourceLoc elseLoc,
         Stmt *elseStmt)
      : ConditionalStmt(StmtKind::If, cond), thenStmt(thenStmt),
        elseStmt(elseStmt), ifLoc(ifLoc), elseLoc(elseLoc) {}

  /// \param ifLoc the SourceLoc of the "if" keyword
  /// \param thenStmt the "then" statement
  IfStmt(SourceLoc ifLoc, StmtCondition cond, Stmt *thenStmt)
      : IfStmt(ifLoc, cond, thenStmt, SourceLoc(), nullptr) {}

  SourceLoc getIfLoc() const { return ifLoc; }
  SourceLoc getElseLoc() const { return elseLoc; }

  Stmt *getThen() const { return thenStmt; }
  void setThen(Stmt *stmt) { thenStmt = stmt; }

  Stmt *getElse() const { return elseStmt; }
  void setElse(Stmt *stmt) { elseStmt = stmt; }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const { return ifLoc; }
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const {
    return elseStmt ? elseStmt->getEndLoc() : thenStmt->getEndLoc();
  }

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::If;
  }
};

/// Represents a "while" loop
///
/// \verbatim
/// while cond {
///   /* do something */
/// }
/// \endverbatim
class WhileStmt final : public ConditionalStmt {
  SourceLoc whileLoc;
  Stmt *body = nullptr;

public:
  /// \param whileLoc the SourceLoc of the "while" keyword
  /// \param cond the loop condition
  /// \param body the body of the while loop
  WhileStmt(SourceLoc whileLoc, StmtCondition cond, Stmt *body)
      : ConditionalStmt(StmtKind::While, cond), whileLoc(whileLoc), body(body) {
  }

  SourceLoc getWhileLoc() const { return whileLoc; }

  Stmt *getBody() const { return body; }
  void setBody(Stmt *body) { this->body = body; }

  /// \returns the SourceLoc of the first token of the statement
  SourceLoc getBegLoc() const { return whileLoc; }
  /// \returns the SourceLoc of the last token of the statement
  SourceLoc getEndLoc() const { return body->getEndLoc(); }

  static bool classof(const Stmt *stmt) {
    return stmt->getKind() == StmtKind::While;
  }
};
} // namespace sora