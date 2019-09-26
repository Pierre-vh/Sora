//===--- ParseDecl.cpp - Declaration Parsing Impl. --------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/Decl.hpp"
#include "Sora/AST/SourceFile.hpp"
#include "Sora/AST/Stmt.hpp"
#include "Sora/AST/Type.hpp"
#include "Sora/Parser/Parser.hpp"

using namespace sora;

bool Parser::isStartOfDecl() const {
  switch (tok.getKind()) {
  case TokenKind::LetKw:
  case TokenKind::FuncKw:
    return true;
  default:
    return false;
  }
}

/*
source-file = top-level-declaration+
top-level-declaration = function-declaration
                      | type-declaration
                      | struct-declaration
*/
void Parser::parseSourceFile() {
  while (!isEOF()) {
    if (tok.isNot(TokenKind::FuncKw)) {
      diagnose(tok, diag::expected_fn_decl);
      skipUntil(TokenKind::FuncKw);
      continue;
    }
    auto result = parseFuncDecl();
    if (result.isNull()) {
      skipUntil(TokenKind::FuncKw);
      continue;
    }
    sourceFile.addMember(result.get());
  }
}

ParserResult<ParamDecl> Parser::parseParamDecl() { return nullptr; }

ParserResult<ParamList> Parser::parseParamDeclList() {
  assert(tok.is(TokenKind::LParen) && "not a param list!");
  SourceLoc lParenLoc = consumeToken();
  // TODO
  skip();
  return makeParserResult(ParamList::createEmpty(ctxt, lParenLoc, lParenLoc));
}

/*
function-declaration = "func" identifier parameter-declaration-list
                      ("->" type)? block-statement
*/
ParserResult<FuncDecl> Parser::parseFuncDecl() {
  assert(tok.is(TokenKind::FuncKw) && "Not a func!");
  SourceLoc fnLoc = consumeToken();

  // identifier
  Identifier identifier;
  SourceLoc identifierLoc;
  if (tok.isIdentifier())
    identifierLoc = consumeIdentifier(identifier);
  else {
    diagnoseExpected(diag::expected_ident_in_fn);
    return nullptr;
  }

  // parameter-declaration-list
  ParamList *paramList = nullptr;
  if (tok.is(TokenKind::LParen)) {
    auto paramListResult = parseParamDeclList();
    if (paramListResult.isNull())
      return nullptr;
    paramList = paramListResult.get();
  }
  else {
    diagnoseExpected(diag::expected_lparen_in_fn_arg_list);
    return nullptr;
  }

  // ("->" type)?
  TypeLoc retTL;
  if (consumeIf(TokenKind::Arrow)) {
    auto result =
        parseType([&]() { diagnoseExpected(diag::expected_fn_ret_ty); });
    if (result.isNull())
      return nullptr;
    retTL = TypeLoc(result.get());
  }

  auto node = new (ctxt)
      FuncDecl(declContext, fnLoc, identifierLoc, identifier, paramList, retTL);

  // block-statement
  if (consumeIf(TokenKind::LCurly)) {
    // Set the DeclContext for the parsing of the body.
    auto bodyDC = setDeclContextRAII(node);
    // Parse the body
    auto result = parseBlockStmt();
    if (result.isNull())
      return nullptr;
    node->setBody(result.get());
  }
  else {
    diagnoseExpected(diag::expected_lcurly_fn_body);
    return nullptr;
  }

  return makeParserResult(node);
}