//===--- ParserResult.hpp - Parser Result -----------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/PointerIntPair.h"

namespace sora {
template <typename T> class ParserResult final {
  /// The Result + a flag indicating if it's an error result.
  llvm::PointerIntPair<T *, 1> resultAndIsSuccess;

  ParserResult(T *ptr, bool isSucccess) : resultAndIsSuccess(ptr, isSucccess) {}

public:
  /// Constructor to allow implict upcast, e.g. cast a ParserResult<BinaryExpr>
  /// to ParserResult<Expr>.
  template <typename U, typename = typename std::enable_if<
                            std::is_base_of<T, U>::value, U>::type>
  ParserResult(ParserResult<U> &&derived)
      : ParserResult(derived.getOrNull(), derived.isSuccess()) {}
  // Constructor for error parser results
  ParserResult(std::nullptr_t) : ParserResult(nullptr, false) {}
  /// Constructor for successful parser results. \p result cannot be nullptr.
  explicit ParserResult(T *result) : ParserResult(result, true) {
    assert(result && "Successful Parser Results can't be nullptr!");
  }

  /// Sets the "error" flag to nullptr.
  void setIsError() { resultAndIsSuccess.setInt(false); }
  /// \returns true if this represents a parsing error
  bool isError() const { return !isSuccess(); }
  /// \returns true if this represents a parsing success
  bool isSuccess() const { return resultAndIsSuccess.getInt(); }

  /// \returns true if this ParserResult has a null result
  bool isNull() const { return getOrNull() == nullptr; }

  /// \returns the result (can't be nullptr)
  T *get() const {
    T *ptr = getOrNull();
    assert(ptr && "result can't be nullptr!");
    return ptr;
  }

  /// \returns the result or nullptr (if error)
  T *getOrNull() const { return resultAndIsSuccess.getPointer(); }

  /// \returns isSuccess()
  explicit operator bool() const { return isSuccess(); }
};

/// Creates a successful parser result
template <typename T>
static inline ParserResult<T> makeParserResult(T *result) {
  return ParserResult<T>(result);
}

/// Creates a parser error result
template <typename T>
static inline ParserResult<T> makeParserErrorResult(T *result) {
  if (!result)
    return ParserResult<T>(nullptr);
  ParserResult<T> pr(result);
  pr.setIsError();
  return pr;
}
} // namespace sora