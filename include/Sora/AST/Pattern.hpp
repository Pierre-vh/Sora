//===--- Pattern.hpp - Pattern ASTs -----------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/ASTAlignement.hpp"
#include "Sora/AST/Identifier.hpp"
#include "Sora/AST/Type.hpp"
#include "Sora/Common/LLVM.hpp"
#include "Sora/Common/SourceLoc.hpp"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/TrailingObjects.h"
#include <cassert>

#include <stdint.h>

namespace sora {
class ASTContext;
class ASTWalker;
class VarDecl;

/// Kinds of Patterns
enum class PatternKind : uint8_t {
#define PATTERN(KIND, PARENT) KIND,
#include "Sora/AST/PatternNodes.def"
};

/// Base class for every Pattern node
class alignas(PatternAlignement) Pattern {
  // Disable vanilla new/delete for patterns
  void *operator new(size_t) noexcept = delete;
  void operator delete(void *)noexcept = delete;

  Type type;
  PatternKind kind;
  /// Make use of the padding bits by allowing derived class to store data here.
  /// NOTE: Derived classes are expected to initialize the bitfield themselves.
  LLVM_PACKED(union Bits {
    Bits() : raw() {}
    // Raw bits (to zero-init the union)
    char raw[7];
    // TuplePattern
    struct {
      uint32_t numElements;
    } tuplePattern;
  });
  static_assert(sizeof(Bits) == 7, "Bits is too large!");

protected:
  Bits bits;

  // Children should be able to use placement new, as it is needed for children
  // with trailing objects.
  void *operator new(size_t, void *mem) noexcept {
    assert(mem);
    return mem;
  }

  Pattern(PatternKind kind) : kind(kind) {}

public:
  // Publicly allow allocation of patterns using the ASTContext.
  void *operator new(size_t size, ASTContext &ctxt,
                     unsigned align = alignof(Pattern));

  /// Traverse this Pattern using \p walker.
  /// \returns true if the walk completed successfully, false if it ended
  /// prematurely.
  bool walk(ASTWalker &walker);
  bool walk(ASTWalker &&walker) { return walk(walker); }

  /// Dumps this statement to \p out
  void dump(raw_ostream &out, const SourceManager &srcMgr,
            unsigned indent = 2) const;

  /// \return the kind of patterns this is
  PatternKind getKind() const { return kind; }

  bool hasType() const { return !type.isNull(); }
  Type getType() const { return type; }
  void setType(Type type) { this->type = type; }

  /// Skips parentheses around this Pattern: If this is a ParenPattern, returns
  /// getSubPattern()->ignoreParens(), else returns this.
  Pattern *ignoreParens();
  const Pattern *ignoreParens() const {
    return const_cast<Pattern *>(this)->ignoreParens();
  }

  /// Call \p fn on each VarDecl in this Pattern.
  void forEachVarDecl(llvm::function_ref<void(VarDecl *)> fn) const;

  /// \returns the SourceLoc of the first token of the pattern
  SourceLoc getBegLoc() const;
  /// \returns the SourceLoc of the last token of the pattern
  SourceLoc getEndLoc() const;
  /// \returns the preffered SourceLoc for diagnostics. This is defaults to
  /// getBegLoc but nodes can override it as they please.
  SourceLoc getLoc() const;
  /// \returns the full range of this pattern
  SourceRange getSourceRange() const;
};

/// We should only use 16 bytes (2 pointers) max in 64 bits mode because we only
/// store the type, the kind + some packed bits in the base class.
static_assert(sizeof(Pattern) <= 16, "Pattern is too big!");

/// Represents a single variable pattern, which matches any argument and binds
/// it to a name. This is the node that contains VarDecl nodes. A VarDecl* must
/// be given when constructing the object and it can't be changed afterwards.
class VarPattern final : public Pattern {
  VarDecl *const varDecl = nullptr;

public:
  VarPattern(VarDecl *varDecl) : Pattern(PatternKind::Var), varDecl(varDecl) {}

  VarDecl *getVarDecl() const { return varDecl; }
  Identifier getIdentifier() const;

  SourceLoc getBegLoc() const;
  SourceLoc getEndLoc() const;

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Var;
  }
};

/// Represents a wildcard pattern, which matches any argument and discards it.
/// It is spelled '_'.
class DiscardPattern final : public Pattern {
  SourceLoc loc;

public:
  DiscardPattern(SourceLoc loc) : Pattern(PatternKind::Discard), loc(loc) {}

  SourceLoc getBegLoc() const { return loc; }
  SourceLoc getEndLoc() const { return loc; }

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Discard;
  }
};

/// Represents a "mut" pattern which indicates that, in the following pattern,
/// every variable introduced should be mutable.
class MutPattern final : public Pattern {
  SourceLoc mutLoc;
  Pattern *subPattern = nullptr;

public:
  MutPattern(SourceLoc mutLoc, Pattern *subPattern)
      : Pattern(PatternKind::Mut), mutLoc(mutLoc), subPattern(subPattern) {}

  SourceLoc getMutLoc() const { return mutLoc; }
  Pattern *getSubPattern() const { return subPattern; }

  SourceLoc getBegLoc() const { return mutLoc; }
  SourceLoc getLoc() const { return subPattern->getLoc(); }
  SourceLoc getEndLoc() const { return subPattern->getEndLoc(); }

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Mut;
  }
};

/// Represents a pattern that only consists of grouping parentheses around
/// another pattern.
class ParenPattern final : public Pattern {
  SourceLoc lParenLoc, rParenLoc;
  Pattern *subPattern = nullptr;

public:
  ParenPattern(SourceLoc lParenLoc, Pattern *subPattern, SourceLoc rParenLoc)
      : Pattern(PatternKind::Paren), lParenLoc(lParenLoc), rParenLoc(rParenLoc),
        subPattern(subPattern) {}

  Pattern *getSubPattern() const { return subPattern; }

  SourceLoc getLParenLoc() const { return lParenLoc; }
  SourceLoc getRParenLoc() const { return rParenLoc; }

  SourceLoc getBegLoc() const { return lParenLoc; }
  SourceLoc getLoc() const { return subPattern->getLoc(); }
  SourceLoc getEndLoc() const { return rParenLoc; }

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Paren;
  }
};

/// Represents a Tuple pattern, which is a group of 0 or more patterns
/// in parentheses. Note that there are no single-element tuples, so patterns
/// like "(mut x)" are simply represented using ParenPattern.
class TuplePattern final
    : public Pattern,
      private llvm::TrailingObjects<TuplePattern, Pattern *> {
  friend llvm::TrailingObjects<TuplePattern, Pattern *>;

  SourceLoc lParenLoc, rParenLoc;

  TuplePattern(SourceLoc lParenLoc, ArrayRef<Pattern *> patterns,
               SourceLoc rParenLoc)
      : Pattern(PatternKind::Tuple), lParenLoc(lParenLoc),
        rParenLoc(rParenLoc) {
    assert(patterns.size() != 1 &&
           "Single-element tuples don't exist - Use ParenPattern!");
    bits.tuplePattern.numElements = patterns.size();
    std::uninitialized_copy(patterns.begin(), patterns.end(),
                            getTrailingObjects<Pattern *>());
  }

public:
  /// Creates a TuplePattern. Note that \p patterns must contain either zero
  /// elements, or 2+ elements. It can't contain a single element as one-element
  /// tuples don't exist in Sora (There's no way to write them). Things like
  /// "(pattern)" are represented using a ParenPattern instead.
  static TuplePattern *create(ASTContext &ctxt, SourceLoc lParenLoc,
                              ArrayRef<Pattern *> patterns,
                              SourceLoc rParenLoc);

  /// Creates an empty tuple pattern
  static TuplePattern *createEmpty(ASTContext &ctxt, SourceLoc lParenLoc,
                                   SourceLoc rParenLoc) {
    return create(ctxt, lParenLoc, {}, rParenLoc);
  }

  bool isEmpty() const { return getNumElements() == 0; }
  size_t getNumElements() const { return bits.tuplePattern.numElements; }
  ArrayRef<Pattern *> getElements() const {
    return {getTrailingObjects<Pattern *>(), getNumElements()};
  }
  MutableArrayRef<Pattern *> getElements() {
    return {getTrailingObjects<Pattern *>(), getNumElements()};
  }

  SourceLoc getLParenLoc() const { return lParenLoc; }
  SourceLoc getRParenLoc() const { return rParenLoc; }

  SourceLoc getBegLoc() const { return lParenLoc; }
  SourceLoc getEndLoc() const { return rParenLoc; }

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Tuple;
  }
};

/// Represents a "typed" pattern, which is a pattern followed by a type
/// annotation
///
/// \verbatim
///   a : i32
///   (a, b) : (i32, i32)
/// \endverbatim
class TypedPattern final : public Pattern {
  Pattern *subPattern;
  TypeRepr *typeRepr;

public:
  TypedPattern(Pattern *subPattern, TypeRepr *typeRepr)
      : Pattern(PatternKind::Typed), subPattern(subPattern),
        typeRepr(typeRepr) {}

  Pattern *getSubPattern() const { return subPattern; }
  TypeRepr *getTypeRepr() const { return typeRepr; }

  SourceLoc getBegLoc() const;
  SourceLoc getLoc() const { return subPattern->getLoc(); }
  SourceLoc getEndLoc() const;

  static bool classof(const Pattern *pattern) {
    return pattern->getKind() == PatternKind::Typed;
  }
};
} // namespace sora