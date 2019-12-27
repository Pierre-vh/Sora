//===--- ConstraintSystem.hpp -----------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/Types.hpp"
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
class alignas(alignof(TypeVariableType)) TypeVariableInfo final {
  friend class ConstraintSystem;

  // Disable vanilla new/delete for TypeVariableInfo
  void *operator new(size_t) noexcept = delete;
  void operator delete(void *)noexcept = delete;

  void *operator new(size_t, void *mem) noexcept {
    assert(mem);
    return mem;
  }

  TypeVariableInfo(TypeVariableKind tvKind) : tvKind(tvKind) {}

  /// The kind of TypeVariable this is
  TypeVariableKind tvKind;
  /// This TypeVariable's substitution.
  /// Note that this can be another TypeVariable in case of an equivalence.
  Type substitution;

  // Checks if \p tv is a valid substitution for this TypeVariable
  bool isValidSubstitutionForIntegerTV(Type type) {
    if (type->isAnyIntegerType())
      return true;
    TypeVariableType *tv = type->getAs<TypeVariableType>();
    if (!tv)
      return false;
    // Allow both integer & general type variables.
    auto &info = TypeVariableInfo::get(tv);
    return info.isIntegerTypeVariable() || info.isGeneralTypeVariable();
  }

  // Checks if \p tv is a valid substitution for this TypeVariable
  bool isValidSubstitutionForFloatTV(Type type) {
    if (type->isAnyFloatType())
      return true;
    TypeVariableType *tv = type->getAs<TypeVariableType>();
    if (!tv)
      return false;
    // Allow both float & general type variables.
    auto &info = TypeVariableInfo::get(tv);
    return info.isFloatTypeVariable() || info.isGeneralTypeVariable();
  }

public:
  /// Make TypeVariableInfo noncopyable, so we don't copy it by mistake.
  TypeVariableInfo(const TypeVariableInfo &) = delete;
  TypeVariableInfo &operator=(const TypeVariableInfo &) = delete;

  /// \returns the TypeVariableInfo object for \p type
  static TypeVariableInfo &get(const TypeVariableType *type) {
    return *reinterpret_cast<TypeVariableInfo *>(
        const_cast<TypeVariableType *>(type) + 1);
  }

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

  /// Sets this TypeVariable's substitution.
  /// \returns false if the substitution was rejected (because there's already a
  /// substitution, or because it's not compatible), true if the substitution
  /// was accepted.
  bool setSubstitution(Type type) {
    if (hasSubstitution())
      return false;
    // Never allow LValues into substitutions
    type = type->getRValue();
    // Check if we're not using this TV as its own subst
    if (TypeVariableType *tv = type->getAs<TypeVariableType>())
      assert(this != &get(tv) &&
             "Type variable using itself as a substitution");
    // Check if the substitution is legal.
    switch (getTypeVariableKind()) {
    case TypeVariableKind::General:
      break;
    case TypeVariableKind::Integer:
      if (!isValidSubstitutionForIntegerTV(type))
        return false;
      break;
    case TypeVariableKind::Float:
      if (!isValidSubstitutionForFloatTV(type))
        return false;
      break;
    }
    substitution = type;
    // If the substitution is a General TV but we aren't, make its kind equal to
    // ours.
    // FIXME: Is it better to do this here or in unify()?
    if (!isGeneralTypeVariable()) {
      if (TypeVariableType *tv = type->getAs<TypeVariableType>()) {
        auto &info = TypeVariableInfo::get(tv);
        if (info.isGeneralTypeVariable())
          info.tvKind = getTypeVariableKind();
      }
    }
    return true;
  }
  /// \returns true if this TypeVariable has a substitution (whether it is
  /// bound)
  bool hasSubstitution() const { return (bool)substitution; }
  /// \returns this TypeVariable's substitution
  Type getSubstitution() const { return substitution; }
};

/// Options for type unification
struct UnificationOptions {
  /// Used to compare 2 built-in types.
  std::function<bool(const BuiltinType *, const BuiltinType *)>
      builtinTypeComparator =
          [](const BuiltinType *a, const BuiltinType *b) { return a == b; };

  /// Whether LValues are ignored.
  /// e.g. if true, unification will succeed for "@lvalue i32" and"i32", if set
  /// to false, it'll fail.
  bool ignoreLValues = true;
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
  /// \param tc the TypeChecker instance
  /// Uses i32 and f32 for the default int/float type variable substitutions.
  ConstraintSystem(TypeChecker &tc)
      : ConstraintSystem(tc, tc.ctxt.i32Type, tc.ctxt.f32Type) {}

  /// \param tc the TypeChecker instance
  /// \param intTVDefault The default type for Integer type variables
  /// \param floatTVDefault The default type for Float type variables
  ConstraintSystem(TypeChecker &tc, Type intTVDefault, Type floatTVDefault)
      : ctxt(tc.ctxt), typeChecker(tc),
        raiiCSArena(ctxt.createConstraintSystemArena()),
        intTVDefault(intTVDefault), floatTVDefault(floatTVDefault) {
    assert(intTVDefault &&
           "No default substitution for integer type variables");
    assert(floatTVDefault &&
           "No default substitution for float type variables");
  }

  ConstraintSystem(const ConstraintSystem &) = delete;
  ConstraintSystem &operator=(const ConstraintSystem &) = delete;

  ASTContext &ctxt;
  TypeChecker &typeChecker;

private:
  /// The Constraint System Arena RAII Object.
  RAIIConstraintSystemArena raiiCSArena;

  /// The list of type variables created by this ConstraintSystem. Mostly used
  /// by dumpTypeVariables().
  SmallVector<TypeVariableType *, 8> typeVariables;

  /// The default type of int type variables
  const Type intTVDefault;
  /// The default type of float type variables
  const Type floatTVDefault;

  /// Creates a new type variable of kind \p kind
  TypeVariableType *createTypeVariable(TypeVariableKind kind);

public:
  /// Creates a new General type variable.
  TypeVariableType *createGeneralTypeVariable() {
    return createTypeVariable(TypeVariableKind::General);
  }
  /// Create a new Integer type variable
  TypeVariableType *createIntegerTypeVariable() {
    return createTypeVariable(TypeVariableKind::Integer);
  }
  /// Create a new Float type variable
  TypeVariableType *createFloatTypeVariable() {
    return createTypeVariable(TypeVariableKind::Float);
  }

  /// \returns the default substitution for integer type variables
  Type getIntegerTypeVariableDefaultType() const { return intTVDefault; }
  /// \returns the default substitution for float type variables
  Type getFloatTypeVariableDefaultType() const { return floatTVDefault; }

  /// Simplifies \p type, replacing type variables with their substitutions.
  /// If a general type variable has no substitution, an ErrorType is used, if
  /// an Integer or Float type variable has no substitution, the default type
  /// for those type variables is used (see \c getIntegerTypeVariableDefaultType
  /// and \c getFloatTypeVariableDefaultType)
  ///
  /// \param type the type to simplify
  /// \param hadUnboundTypeVariable whether the type contained an unbound
  /// general type variable.
  /// \returns the simplified type, or the ErrorType on error (never nullptr)
  Type simplifyType(Type type, bool *hadUnboundTypeVariable = nullptr);

  /// Unifies \p a with \p b with \p options.
  /// \returns true if unification was successful, false otherwise.
  bool unify(Type a, Type b,
             const UnificationOptions &options = UnificationOptions());

  /// Prints \p type and its TypeVariableInfo to \p out
  void print(raw_ostream &out, const TypeVariableType *type,
             const TypePrintOptions &printOptions = TypePrintOptions()) const;

  void dumpTypeVariables(
      raw_ostream &out,
      const TypePrintOptions &printOptions = TypePrintOptions()) const;

  /// Same as \c print, but returns a string instead of printing to a stream.
  std::string
  getString(const TypeVariableType *type,
            const TypePrintOptions &printOptions = TypePrintOptions()) const;
};
} // namespace sora