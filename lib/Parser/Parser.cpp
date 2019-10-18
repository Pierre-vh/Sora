//===--- Parser.cpp ---------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//
// Implementation of Parser methods which are not related to type, expr, stmt,
// decl and pattern parsing.
//===----------------------------------------------------------------------===//

#include "Sora/Parser/Parser.hpp"
#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/SourceFile.hpp"
#include "Sora/Lexer/Lexer.hpp"

using namespace sora;

Parser::Parser(ASTContext &ctxt, SourceFile &file)
    : ctxt(ctxt), diagEng(ctxt.diagEngine), sourceFile(file),
      declContext(&file),
      lexer(ctxt.srcMgr, sourceFile.getBufferID(), &diagEng) {
  tok = lexer.lex();
}

const Token &Parser::peek() const { return lexer.peek(); }

SourceLoc Parser::parseMatchingToken(SourceLoc lLoc, TokenKind kind,
                                     Optional<TypedDiag<>> customErr) {
  auto doIt = [&](TypedDiag<> defaultErr, TypedDiag<> note) {
    SourceLoc loc = consumeIf(kind);
    if (!loc) {
      diagnoseExpected(customErr ? *customErr : defaultErr);
      diagnose(lLoc, note);
    }
    return loc;
  };

  switch (kind) {
  default:
    llvm_unreachable("not a matching token");
  case TokenKind::RParen:
    return doIt(diag::expected_rparen, diag::to_match_lparen);
  case TokenKind::RCurly:
    return doIt(diag::expected_rcurly, diag::to_match_lcurly);
  case TokenKind::RSquare:
    return doIt(diag::expected_rsquare, diag::to_match_lsquare);
  }
}

void Parser::parseList(llvm::function_ref<bool(unsigned)> callback) {
  unsigned idx = 0;
  do  {
    // eat extra commas
    while (SourceLoc extraComma = consumeIf(TokenKind::Comma))
      diagnose(extraComma, diag::unexpected_sep, ",");
    // Parse
    if (!callback(idx++))
      return;
  } while (consumeIf(TokenKind::Comma));
}

SourceLoc Parser::consumeToken() {
  assert(tok.isNot(TokenKind::EndOfFile) && "Consuming EOF!");
  SourceLoc loc = tok.getLoc();
  prevTokPastTheEnd = tok.getCharRange().getEnd();
  tok = lexer.lex();
  return loc;
}

SourceLoc Parser::consumeIdentifier(Identifier &identifier) {
  assert(tok.isIdentifier());
  identifier = getIdentifier(tok);
  return consumeToken();
}

void Parser::skip() {
  auto kind = tok.getKind();
  consumeToken();
  switch (kind) {
  case TokenKind::LCurly:
    skipUntil(TokenKind::RCurly);
    consumeIf(TokenKind::RCurly);
    return;
  case TokenKind::LSquare:
    skipUntil(TokenKind::RSquare);
    consumeIf(TokenKind::RSquare);
    return;
  case TokenKind::LParen:
    skipUntil(TokenKind::RParen);
    consumeIf(TokenKind::RParen);
    return;
  default:
    return;
  }
}

void Parser::skipUntil(TokenKind kind) {
  while (tok.isNot(kind, TokenKind::EndOfFile))
    skip();
}

void Parser::skipUntilDecl() {
  while (!isEOF()) {
    if (isStartOfDecl())
      return;
    skip();
  }
}

void Parser::skipUntilTokOrNewline(TokenKind kind) {
  while (!isEOF()) {
    if (tok.is(kind) || tok.isAtStartOfLine())
      return;
    skip();
  }
}

void Parser::skipUntilTokDeclRCurly(TokenKind kind) {
  while (!isEOF()) {
    if (isStartOfDecl() || tok.isAny(kind, TokenKind::RCurly))
      return;
    skip();
  }
}

void Parser::skipUntilTokDeclStmtRCurly(TokenKind kind) {
  while (!isEOF()) {
    if (isStartOfDecl() || isStartOfStmt() ||
        tok.isAny(kind, TokenKind::RCurly))
      return;
    skip();
  }
}

Identifier Parser::getIdentifier(const Token &tok) {
  return ctxt.getIdentifier(tok.str());
}

int Parser::getColumnDifference(SourceLoc a, SourceLoc b) const {
  const SourceManager &srcMgr = ctxt.srcMgr;
  return srcMgr.getLineAndColumn(b).second - srcMgr.getLineAndColumn(a).second;
}