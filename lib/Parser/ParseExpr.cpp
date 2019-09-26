//===--- ParseExpr.cpp - Expression Parsing Impl. ---------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/Expr.hpp"
#include "Sora/Parser/Parser.hpp"

using namespace sora;

ParserResult<Expr> Parser::parseExpr(const std::function<void()> &onNoExpr) {
  // !TODO!
  return makeParserResult(new (ctxt) ErrorExpr(SourceRange()));
}