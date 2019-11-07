//===--- NameLookup.cpp -----------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/NameLookup.hpp"
#include "Sora/AST/ASTScope.hpp"
#include "Sora/AST/Decl.hpp"
#include "Sora/AST/SourceFile.hpp"
#include "Sora/AST/Stmt.hpp"

using namespace sora;

//===- ASTScope Lookup Implementation -------------------------------------===//

namespace {
struct ASTScopeLookup {
  using LookupResultConsumer = ASTScope::LookupResultConsumer;

  ASTScopeLookup(LookupResultConsumer consumer, Identifier ident)
      : consumerFunc(consumer), ident(ident) {}

  LookupResultConsumer consumerFunc;
  Identifier ident;

  void visit(const ASTScope *scope) {
    bool stop;
    switch (scope->getKind()) {
#define SCOPE(KIND)                                                            \
  case ASTScopeKind::KIND:                                                     \
    stop = visit##KIND(static_cast<const KIND##Scope *>(scope));               \
    break;
#include "Sora/AST/ASTScopeKinds.def"
    }
    if (stop)
      return;
    if (ASTScope *parent = scope->getParent())
      visit(parent);
  }

  /// Calls the consumer if \p decls contains at least one decl.
  /// \returns the result of the consumer if it was called, false otherwise.
  bool consume(ArrayRef<ValueDecl *> decls, const ASTScope *scope) {
    return decls.empty() ? false : consumerFunc(decls, scope);
  }

  /// \returns true if \p decl should be considered as a result
  bool shouldConsider(ValueDecl *decl) {
    return considerEveryResult() ? true : decl->getIdentifier() == ident;
  }

  /// \returns true if we are looking for any decl, no matter the name.
  bool considerEveryResult() const { return !ident.isValid(); }

  //===- Visit Methods ----------------------------------------------------===//
  // Visit Methods visit a single scope, and return true when lookup can stop.
  //===--------------------------------------------------------------------===//

  bool visitSourceFile(const SourceFileScope *scope) {
    // Check the contents of the file

    // If we can consider every result, just feed everything to consume()
    SourceFile &sf = scope->getSourceFile();
    if (considerEveryResult())
      return consume(sf.getMembers(), scope);

    // If we can't consider everything, remove everything that can't be
    // considered.
    // FIXME: Ideally, the SourceFile should have a dedicated lookup cache &
    // lookup function.
    SmallVector<ValueDecl *, 4> decls;
    for (ValueDecl *decl : sf.getMembers()) {
      if (shouldConsider(decl))
        decls.push_back(decl);
    }
    return consume(decls, scope);
  }

  bool visitLocalLetDecl(const LocalLetDeclScope *scope) {
    // Collect every variable declared in the "let" and feed them to the
    // consumer.
    SmallVector<ValueDecl *, 4> decls;
    scope->getLetDecl()->forEachVarDecl([&](VarDecl *var) {
      if (shouldConsider(var))
        decls.push_back(var);
    });
    return consume(decls, scope);
  }

  bool visitFuncDecl(const FuncDeclScope *scope) {
    // Consider the params of the function
    // FIXME: Can this be made more efficient?
    FuncDecl *fn = scope->getFuncDecl();
    SmallVector<ValueDecl *, 4> decls;
    for (ParamDecl *param : *fn->getParamList()) {
      if (shouldConsider(param))
        decls.push_back(param);
    }
    return consume(decls, scope);
  }

  bool visitBlockStmt(const BlockStmtScope *scope) {
    // We only need to search for local func declarations.
    // FIXME: Can this be made more efficient? e.g. by automatically skipping
    // BlockStmts known to contain no FuncDecls.
    // Perhaps it'd be a good idea to add a "hasFuncDecl" flag to BlockStmt
    // (which would be extended to "hasFuncOrTypeDecl") later.
    SmallVector<ValueDecl *, 4> decls;
    for (ASTNode node : scope->getBlockStmt()->getElements()) {
      Decl *decl = node.dyn_cast<Decl *>();
      if (!decl)
        continue;
      FuncDecl *func = dyn_cast<FuncDecl>(decl);
      if (func && shouldConsider(func))
        decls.push_back(func);
    }
    return consume(decls, scope);
  }

  bool visitIfStmt(const IfStmtScope *scope) {
    // Nothing to do (if the cond declares something, it'll be handled by the
    // implicit LocalLetDeclScope)
    return false;
  }

  bool visitWhileStmt(const WhileStmtScope *scope) {
    // Nothing to do (if the cond declares something, it'll be handled by the
    // implicit LocalLetDeclScope)
    return false;
  }
};
} // namespace

//===- ASTScope::lookup ---------------------------------------------------===//

void ASTScope::lookup(ASTScope::LookupResultConsumer consumer,
                      Identifier ident) const {
  ASTScopeLookup(consumer, ident).visit(this);
}

//===- UnqualifiedLookup --------------------------------------------------===//

namespace {
/// The lookup results consumer for UnqualifiedLookup
/// \returns true if lookup can stop, false otherwise.
bool unqualifiedLookupConsumer(SmallVectorImpl<ValueDecl *> &results,
                               ArrayRef<ValueDecl *> candidates,
                               const ASTScope *scope, bool stopAtFirstResults) {
  assert(!candidates.empty() && "Candidates set shouldn't be empty");
  // Simply add the candidates to the set of results
  results.append(candidates.begin(), candidates.end());
  // Stop looking unless we are told to keep going (e.g. when we're trying to
  // find every visible names)
  return stopAtFirstResults;
}
} // namespace

void UnqualifiedLookup::lookupImpl(SourceLoc loc, Identifier ident) {
  ASTScope *innermostScope = sourceFile.getScopeMap()->findInnermostScope(loc);
  assert(innermostScope && "findInnermostScope shouldn't return null!");
  auto consumer = [&](ArrayRef<ValueDecl *> candidates,
                      const ASTScope *scope) -> bool {
    return unqualifiedLookupConsumer(results, candidates, scope,
                                     /*stopAtFirstResults*/ ident.isValid());
  };
  ASTScopeLookup(consumer, ident).visit(innermostScope);
}