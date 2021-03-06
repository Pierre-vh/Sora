//===--- ASTScope.cpp -------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/ASTScope.hpp"
#include "ASTNodeLoc.hpp"
#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/ASTVisitor.hpp"
#include "Sora/AST/Decl.hpp"
#include "Sora/AST/SourceFile.hpp"
#include "Sora/AST/Stmt.hpp"
#include "Sora/Lexer/Lexer.hpp"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

using namespace sora;

//===- ASTScope -----------------------------------------------------------===//

void *ASTScope::operator new(size_t size, ASTContext &ctxt, unsigned align) {
  return ctxt.allocate(size, align, ArenaKind::Permanent);
}

/// \returns true if \p other's range doesn't begin or end inside this scope's
/// range.
bool ASTScope::overlaps(const ASTScope *other) const {
  assert(other && "other is null");
  SourceRange thisRange = getSourceRange();
  SourceRange otherRange = other->getSourceRange();
  // Check that the beginning of otherRange isn't inside this range
  if (thisRange.begin <= otherRange.begin && otherRange.begin <= thisRange.end)
    return true;
  // Check that the end of otherRange isn't inside this range
  return thisRange.begin <= otherRange.end && otherRange.end <= thisRange.end;
}

void ASTScope::addChild(ASTScope *scope) {
  assert(scope && "adding an empty scope!");
#ifndef NDEBUG
  assert((scope->getBegLoc() >= getBegLoc()) &&
         (scope->getEndLoc() <= getEndLoc() &&
          "Children scope's range is greater than its parent's"));
#endif
  // \returns true if the range of \p first comes before \p second.
  // This also checks that the ranges don't overlap.
  static auto comparator = [](const ASTScope *first, const ASTScope *second) {
    assert(first && second && "Null scope!");
    SourceRange firstRange = first->getSourceRange();
    SourceRange secondRange = second->getSourceRange();
    assert(!first->overlaps(second) && "Scopes can't overlap!");
    return firstRange.end < secondRange.begin;
  };
  auto it = children.end();
  // If the element must be inserted somewhere else than at the end of the
  // vector, use std::lower_bound to find the insertion position
  if (!children.empty() && !comparator(children.back(), scope))
    std::upper_bound(children.begin(), children.end(), scope, comparator);
  children.insert(it, scope);

  // Check if we now need cleanups
  if (!hasCleanup && needsCleanup()) {
    getASTContext().addDestructorCleanup(*this);
    hasCleanup = true;
  }
}

ASTScope *ASTScope::findInnermostScope(SourceLoc loc) {
  expand();

  // Find the children in which loc belongs.
  // We can use std::lower_bound for this with a custom comparator.
  static auto comparator = [](ASTScope *scope, SourceLoc loc) {
    return scope->getSourceRange().end < loc;
  };
  ASTScope *candidate = nullptr;
  auto it = std::lower_bound(children.begin(), children.end(), loc, comparator);
  if (it != children.end())
    candidate = *it;
  // If we got a candidate, check that loc is inside it.
  // FIXME: Is this extra check needed? Won't the candidate always have the loc
  // inside it?
  if (candidate) {
    SourceRange range = candidate->getSourceRange();
    if (range.begin <= loc && loc <= range.end)
      return candidate->findInnermostScope(loc);
  }
  // If there's no candidate just return this.
  return this;
}

void ASTScope::fullyExpand() {
  expand();
  for (ASTScope *child : children)
    child->fullyExpand();
}

SourceLoc ASTScope::getBegLoc() const {
  switch (getKind()) {
#define SCOPE(ID)                                                              \
  case ASTScopeKind::ID:                                                       \
    return ASTNodeLoc<ASTScope, ID##Scope, false>::getBegLoc(                  \
        cast<ID##Scope>(this));
#include "Sora/AST/ASTScopeKinds.def"
  }
  llvm_unreachable("unknown ASTScopeKind");
}

SourceLoc ASTScope::getEndLoc() const {
  switch (getKind()) {
#define SCOPE(ID)                                                              \
  case ASTScopeKind::ID:                                                       \
    return ASTNodeLoc<ASTScope, ID##Scope, false>::getEndLoc(                  \
        cast<ID##Scope>(this));
#include "Sora/AST/ASTScopeKinds.def"
  }
  llvm_unreachable("unknown ASTScopeKind");
}

SourceRange ASTScope::getSourceRange() const {
  switch (getKind()) {
#define SCOPE(ID)                                                              \
  case ASTScopeKind::ID:                                                       \
    return ASTNodeLoc<ASTScope, ID##Scope, false>::getSourceRange(             \
        cast<ID##Scope>(this));
#include "Sora/AST/ASTScopeKinds.def"
  }
  llvm_unreachable("unknown ASTScopeKind");
}

ASTContext &ASTScope::getASTContext() const {
  if (const SourceFileScope *scope = dyn_cast<const SourceFileScope>(this))
    return scope->getSourceFile().astContext;
  ASTScope *parent = getParent();
  assert(parent && "ASTScope should always have a parent");
  return parent->getASTContext();
}

namespace {

void expandSourceFileScope(SourceFileScope *scope) {
  // Create a scope for each FuncDecl inside the SourceFile.
  for (Decl *decl : scope->getSourceFile().getMembers()) {
    if (FuncDecl *fn = dyn_cast<FuncDecl>(decl))
      scope->addChild(FuncDeclScope::create(fn, scope));
  }
}

void expandBlockStmtScope(BlockStmtScope *scope) {
  ASTContext &ctxt = scope->getASTContext();
  BlockStmt *block = scope->getBlockStmt();
  ASTScope *curParent = scope;
  SourceLoc blockEnd = block->getEndLoc();

  for (BlockStmtElement elt : block->getElements()) {
    // We want to create scopes for everything interesting inside
    // the body:
    if (Stmt *stmt = elt.dyn_cast<Stmt *>()) {
      // If, Block and Whiles are interesting.
      ASTScope *result = nullptr;
      if (IfStmt *ifStmt = dyn_cast<IfStmt>(stmt))
        result = IfStmtScope::create(ctxt, ifStmt, curParent);
      else if (WhileStmt *whileStmt = dyn_cast<WhileStmt>(stmt))
        result = WhileStmtScope::create(ctxt, whileStmt, curParent);
      else if (BlockStmt *block = dyn_cast<BlockStmt>(stmt))
        result = BlockStmtScope::create(ctxt, block, curParent);
      else
        continue;
      curParent->addChild(result);
    }
    else if (Decl *decl = elt.dyn_cast<Decl *>()) {
      // FuncDecls and LetDecls are interesting. Especially LetDecls.
      if (FuncDecl *func = dyn_cast<FuncDecl>(decl)) {
        // For FuncDecls, just create a FuncDeclScope.
        curParent->addChild(FuncDeclScope::create(func, curParent));
        continue;
      }
      LetDecl *let = dyn_cast<LetDecl>(decl);
      if (!let)
        continue;
      // For LetDecls, create a new scope and make that scope the current
      // parent.
      assert(let->isLocal() && "not local?!");
      // The beg loc of the scope will be the token past-the-end of the LetDecl.
      SourceLoc begLoc =
          Lexer::getLocPastTheEndOfTokenAtLoc(ctxt.srcMgr, let->getEndLoc());
      ASTScope *scope =
          LocalLetDeclScope::create(let, curParent, {begLoc, blockEnd});
      curParent->addChild(scope);
      curParent = scope;
    }
    // exprs aren't interesting
  }
}

ASTScope *createConditionBodyScope(ASTContext &ctxt, ASTScope *parent,
                                   StmtCondition cond, BlockStmt *body) {
  ASTScope *scope = nullptr;
  if (LetDecl *let = cond.getLetDeclOrNull()) {
    scope = LocalLetDeclScope::create(let, parent, body->getSourceRange());
    scope->addChild(BlockStmtScope::create(ctxt, body, scope));
  }
  else
    scope = BlockStmtScope::create(ctxt, body, parent);
  return scope;
}

void expandIfStmtScope(IfStmtScope *scope) {
  // Create a scope for the then
  IfStmt *stmt = scope->getIfStmt();
  ASTContext &ctxt = scope->getASTContext();
  scope->addChild(
      createConditionBodyScope(ctxt, scope, stmt->getCond(), stmt->getThen()));
  // (maybe) create a scope for the else
  if (Stmt *elseStmt = stmt->getElse()) {
    // If it's a block, just create a BlockStmtScope
    if (BlockStmt *block = dyn_cast<BlockStmt>(elseStmt))
      scope->addChild(BlockStmtScope::create(ctxt, block, scope));
    // else, if it's a if, create another IfStmtScope
    else if (IfStmt *elif = dyn_cast<IfStmt>(elseStmt))
      scope->addChild(IfStmtScope::create(ctxt, elif, scope));
    else
      llvm_unreachable("'else' is not a BlockStmt or a IfStmt?");
  }
}

void expandWhileStmtScope(ASTContext &ctxt, WhileStmtScope *scope) {
  WhileStmt *stmt = scope->getWhileStmt();
  scope->addChild(createConditionBodyScope(scope->getASTContext(), scope,
                                           stmt->getCond(), stmt->getBody()));
}

void expandFuncDeclScope(FuncDeclScope *scope) {
  // Just create a BlockStmtScope for the body
  if (BlockStmt *body = scope->getFuncDecl()->getBody())
    scope->addChild(
        BlockStmtScope::create(scope->getASTContext(), body, scope));
}
} // namespace

void ASTScope::expand() {
  // If we already expanded, we don't need to do anything.
  if (expanded)
    return;
  // Dispatch
  switch (getKind()) {
  case ASTScopeKind::SourceFile: {
    expandSourceFileScope(dyn_cast<SourceFileScope>(this));
    break;
  }
  case ASTScopeKind::BlockStmt: {
    expandBlockStmtScope(dyn_cast<BlockStmtScope>(this));
    break;
  }
  case ASTScopeKind::IfStmt: {
    expandIfStmtScope(dyn_cast<IfStmtScope>(this));
    break;
  }
  case ASTScopeKind::WhileStmt: {
    expandWhileStmtScope(getASTContext(), dyn_cast<WhileStmtScope>(this));
    break;
  }
  case ASTScopeKind::FuncDecl: {
    expandFuncDeclScope(dyn_cast<FuncDeclScope>(this));
    break;
  }
  case ASTScopeKind::LocalLetDecl:
    // nothing to do
    break;
  }
  // Mark this as expanded
  expanded = true;
}

static const char *getKindStr(ASTScopeKind kind) {
  switch (kind) {
#define SCOPE(KIND)                                                            \
  case ASTScopeKind::KIND:                                                     \
    return #KIND;
#include "Sora/AST/ASTScopeKinds.def"
  }
  llvm_unreachable("unknown ASTScope kind");
}

void ASTScope::dump() const { dump(llvm::dbgs()); }

void ASTScope::dumpImpl(raw_ostream &out, unsigned indent,
                        unsigned curIndent) const {
  out.indent(curIndent);
  const SourceManager &srcMgr = getASTContext().srcMgr;
  // Add a "Scope" suffix to the kind string, and print the range as well.
  out << getKindStr(getKind()) << "Scope range:";
  // Only print the source file for SourceFileScopes.
  getSourceRange().print(out, srcMgr, isa<SourceFileScope>(this));
  out << (expanded ? " expanded" : "") << "\n";
  // visit the children
  for (ASTScope *child : getChildren())
    child->dumpImpl(out, indent, curIndent + indent);
}

SourceFileScope *SourceFileScope::create(SourceFile &sf) {
  return new (sf.astContext) SourceFileScope(sf);
}

SourceLoc SourceFileScope::getBegLoc() const { return sourceFile.getBegLoc(); }

SourceLoc SourceFileScope::getEndLoc() const { return sourceFile.getEndLoc(); }

FuncDeclScope *FuncDeclScope::create(FuncDecl *func, ASTScope *parent) {
  return new (func->getASTContext()) FuncDeclScope(func, parent);
}

SourceLoc FuncDeclScope::getBegLoc() const { return decl->getBegLoc(); }

SourceLoc FuncDeclScope::getEndLoc() const { return decl->getEndLoc(); }

bool LocalLetDeclScope::isLocalAndNonNull() const {
  return decl && decl->isLocal();
}

LocalLetDeclScope *LocalLetDeclScope::create(LetDecl *decl, ASTScope *parent,
                                             SourceRange range) {
  return new (decl->getASTContext()) LocalLetDeclScope(decl, parent, range);
}

BlockStmtScope *BlockStmtScope::create(ASTContext &ctxt, BlockStmt *stmt,
                                       ASTScope *parent) {
  return new (ctxt) BlockStmtScope(stmt, parent);
}

SourceLoc BlockStmtScope::getBegLoc() const {
  return getBlockStmt()->getBegLoc();
}

SourceLoc BlockStmtScope::getEndLoc() const {
  return getBlockStmt()->getEndLoc();
}

IfStmtScope *IfStmtScope::create(ASTContext &ctxt, IfStmt *stmt,
                                 ASTScope *parent) {
  return new (ctxt) IfStmtScope(stmt, parent);
}

SourceLoc IfStmtScope::getBegLoc() const { return getIfStmt()->getBegLoc(); }

SourceLoc IfStmtScope::getEndLoc() const { return getIfStmt()->getEndLoc(); }

WhileStmtScope *WhileStmtScope::create(ASTContext &ctxt, WhileStmt *stmt,
                                       ASTScope *parent) {
  return new (ctxt) WhileStmtScope(stmt, parent);
}

SourceLoc WhileStmtScope::getBegLoc() const {
  return getWhileStmt()->getBegLoc();
}

SourceLoc WhileStmtScope::getEndLoc() const {
  return getWhileStmt()->getEndLoc();
}