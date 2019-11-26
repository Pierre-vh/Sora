//===--- ConstraintSystem.hpp -----------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/Type.hpp"
#include "Sora/Common/LLVM.hpp"
#include "TypeChecker.hpp"
#include "llvm/ADT/SmallVector.h"
#include <string>

namespace sora {
class ConstraintSystem;
class TypeVariableType;

/// Kinds of Type Variables
enum class TypeVariableKind : uint8_t {
  /// Can unify w/ any type
  General,
  /// Can unify w/ integer types
  Integer,
  /// Can unify w/ float types
  Float
};

/// TypeVariable extra information
class TypeVariableInfo final {
  friend class ConstraintSystem;

  TypeVariableInfo(TypeVariableKind tvKind) : tvKind(tvKind) {}

  /// The kind of TypeVariable this is
  TypeVariableKind tvKind;
  /// This TypeVariable's substitution.
  /// Note that this can be another TypeVariable in case of an equivalence.
  Type substitution;

public:
  TypeVariableKind getTypeVariableKind() const { return tvKind; }

  bool isGeneralTypeVariable() const {
    return getTypeVariableKind() == TypeVariableKind::General;
  }

  bool isIntegerTypeVariable() const {
    return getTypeVariableKind() == TypeVariableKind::Integer;
  }

  bool isFloatTypeVariable() const {
    return getTypeVariableKind() == TypeVariableKind::Float;
  }

  /// Sets this TypeVariable's substitution. This can only be done once (\c
  /// hasSubstitution() must return false)
  void setSubstitution(Type type) {
    assert(!hasSubstitution() && "Already has a substitution");
    substitution = type;
  }
  /// \returns true if this TypeVariable has a substitution
  bool hasSubstitution() const { return (bool)substitution; }
  /// \returns this TypeVariable's substitution (it can be a TypeVariable in
  /// case of an equivalence class, see \c getBoundSubstitution() to get a
  /// non TypeVariable substitution)
  Type getSubstitution() const { return substitution; }
};

/// The ConstraintSystem serves a context for constraint generation, solving
/// and diagnosing.
///
/// It creates TypeVariables, keeps track of their substitutions, of the
/// current constraints, etc.
///
/// It also possesses a RAIIConstraintSystemArena object, so while this object
/// is alive, the ConstraintSystem arena is available.
///
/// Note that only one of those can be active at once, and that this is a
/// relatively large object that can't be copied.
class ConstraintSystem final {
public:
  ConstraintSystem(TypeChecker &tc)
      : ctxt(tc.ctxt), raiiCSArena(ctxt.createConstraintSystemArena()) {}

  ConstraintSystem(const ConstraintSystem &) = delete;
  ConstraintSystem &operator=(const ConstraintSystem &) = delete;

  ASTContext &ctxt;

private:
  /// The Constraint System Arena RAII Object.
  RAIIConstraintSystemArena raiiCSArena;

  /// The ID of the next TypeVariable that'll be allocated by the constraint
  /// system.
  unsigned nextTypeVariableID = 0;

  /// The array of type variable informations.
  SmallVector<TypeVariableInfo *, 16> typeVariableInfos;

public:
  /// Creates a new type variable.
  TypeVariableType *createTypeVariable();

  /// Simplifies \p type, replacing type variables with their substitution (or
  /// ErrorType if there's no substitution)
  Type simplifyType(Type type);

  /// Prints \p type and its TypeVariableInfo to \p out
  void print(raw_ostream &out, const TypeVariableType *type,
             const TypePrintOptions &printOptions = TypePrintOptions()) const;

  /// Same as \c print, but returns a string instead of printing to a stream.
  std::string
  getString(const TypeVariableType *type,
            const TypePrintOptions &printOptions = TypePrintOptions()) const;

  /// \returns the TypeVariableKind of \p type
  TypeVariableInfo &getInfo(const TypeVariableType *type) const;
};
} // namespace sora