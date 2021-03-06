//===--- NameLookup.hpp - AST Name Lookup Entry Points ----------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/Identifier.hpp"
#include "Sora/AST/Type.hpp"
#include "Sora/Common/LLVM.hpp"
#include "Sora/Common/SourceLoc.hpp"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

namespace sora {
class SourceFile;
class SourceLoc;
class ValueDecl;

struct UnqualifiedLookupOptions {
  /// If non-null, this is called after looking into a scope before looking into
  /// the next one. If it returns true, lookup stops, if it returns true, it
  /// continues.
  std::function<bool(const ASTScope *)> shouldStop;
  /// If true, illegal redeclarations are ignored.
  /// Default is true.
  bool ignoreIllegalRedeclarations = true;
};

/// Class used to configure, execute and collect the results of an unqualified
/// value lookup inside a SourceFile.
class UnqualifiedValueLookup final {

  void lookupImpl(SourceLoc loc, Identifier ident);

  /// Adds \p decls to the list of results.
  /// \returns true if at least one result was added.
  bool addResults(ArrayRef<ValueDecl *> decls) {
    bool added = false;
    for (ValueDecl *decl : decls) {
      if (ignoredDecls.find(decl) == ignoredDecls.end()) {
        results.push_back(decl);
        added = true;
      }
    }
    return added;
  }

  llvm::DenseSet<ValueDecl *> ignoredDecls;

public:
  UnqualifiedValueLookup(SourceFile &sourceFile) : sourceFile(sourceFile) {}

  // Since this object is quite large, make it noncopyable.
  UnqualifiedValueLookup(const UnqualifiedValueLookup &) = delete;
  UnqualifiedValueLookup &operator=(const UnqualifiedValueLookup &) = delete;

  /// Adds \p decls to the list of decls that should be ignored during the
  /// lookup.
  UnqualifiedValueLookup &ignore(ArrayRef<ValueDecl *> decls) {
    ignoredDecls.insert(decls.begin(), decls.end());
    return *this;
  }

  /// Filters the result set, removing every results for which \p filter returns
  /// true.
  UnqualifiedValueLookup &
  filterResults(llvm::function_ref<bool(ValueDecl *)> filter) {
    results.erase(std::remove_if(results.begin(), results.end(), filter),
                  results.end());
    return *this;
  }

  /// Lookup for values with name \p ident at \p loc
  void performLookup(SourceLoc loc, Identifier ident) {
    assert(ident && "identifier is invalid!");
    lookupImpl(loc, ident);
  }

  /// Finds every value visible at \p loc
  void findDeclsAt(SourceLoc loc) { lookupImpl(loc, Identifier()); }

  /// \returns whether the set of results is empty
  bool isEmpty() const { return results.empty(); }
  /// \returns whether the set of results contains a single result
  bool isResultUnique() const { return results.size() == 1; }
  /// \returns the single result of the lookup, or nullptr if there are 0 or 2+
  /// results.
  ValueDecl *getUniqueResult() const {
    return (results.size() == 1) ? results[0] : nullptr;
  }

  /// The lookup options used
  UnqualifiedLookupOptions options;
  /// The SourceFile in which we are looking
  SourceFile &sourceFile;
  /// The list of results
  SmallVector<ValueDecl *, 4> results;
};

/// Class used to configure, execute and collect the results of an unqualified
/// type lookup inside a SourceFile.
class UnqualifiedTypeLookup {
  void lookupImpl(SourceLoc loc, Identifier ident);

public:
  UnqualifiedTypeLookup(SourceFile &sourceFile) : sourceFile(sourceFile) {}

  /// Lookup for types with name \p ident in \p loc
  void performLookup(SourceLoc loc, Identifier ident) {
    assert(ident && "identifier is invalid!");
    lookupImpl(loc, ident);
  }

  /// Finds every type visible at \p loc
  void findTypesAt(SourceLoc loc) { lookupImpl(loc, Identifier()); }

  /// The SourceFile in which we are looking
  SourceFile &sourceFile;
  /// The list of results
  SmallVector<Type, 4> results;
};
} // namespace sora