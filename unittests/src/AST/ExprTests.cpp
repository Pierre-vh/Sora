//===--- ExprTests.cpp ------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/Expr.hpp"
#include "Sora/AST/TypeRepr.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "Sora/Diagnostics/DiagnosticEngine.hpp"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "gtest/gtest.h"

using namespace sora;

namespace {
const char *str = "Hello, World!";

class ExprTest : public ::testing::Test {
protected:
  ExprTest() {
    // Setup locs
    beg = SourceLoc::fromPointer(str);
    mid = SourceLoc::fromPointer(str + 5);
    end = SourceLoc::fromPointer(str + 10);
    Expr *begExpr = new (*ctxt) DiscardExpr(beg);
    Expr *midExpr = new (*ctxt) DiscardExpr(mid);
    Expr *endExpr = new (*ctxt) DiscardExpr(end);
    // Setup nodes
    unresDeclRefExpr = new (*ctxt) UnresolvedDeclRefExpr({}, beg);
    unresMembRefExpr = new (*ctxt)
        UnresolvedMemberRefExpr(unresDeclRefExpr, mid, false, end, {});
    declRefExpr = new (*ctxt) DeclRefExpr(beg, nullptr);
    discardExpr = new (*ctxt) DiscardExpr(beg);
    intLitExpr = new (*ctxt) IntegerLiteralExpr("0", beg);
    fltLitExpr = new (*ctxt) FloatLiteralExpr("0", beg);
    boolLitExpr = new (*ctxt) BooleanLiteralExpr("0", beg);
    nullLitExpr = new (*ctxt) NullLiteralExpr(beg);
    implicitMaybeConvExpr = new (*ctxt) ImplicitMaybeConversionExpr(begExpr);
    destructuredTupleEltExpr =
        new (*ctxt) DestructuredTupleElementExpr({beg, end}, 0);
    loadExpr = new (*ctxt) LoadExpr(begExpr);
    errorExpr = new (*ctxt) ErrorExpr({beg, end});
    castExpr = new (*ctxt)
        CastExpr(begExpr, mid, new (*ctxt) IdentifierTypeRepr(end, {}));
    tupleEltExpr = new (*ctxt) TupleElementExpr(begExpr, mid, false, end, 0);
    tupleExpr = TupleExpr::createEmpty(*ctxt, beg, end);
    parenExpr = new (*ctxt) ParenExpr(beg, midExpr, end);
    callExpr = CallExpr::create(*ctxt, begExpr, {}, end);
    condExpr = new (*ctxt) ConditionalExpr(begExpr, mid, nullptr, {}, endExpr);
    forceUnwrapExpr = new (*ctxt) ForceUnwrapExpr(begExpr, end);
    binaryExpr =
        new (*ctxt) BinaryExpr(begExpr, BinaryOperatorKind::Add, mid, endExpr);
    unaryExpr =
        new (*ctxt) UnaryExpr(UnaryOperatorKind::AddressOf, beg, endExpr);
  }

  SourceManager srcMgr;
  DiagnosticEngine diagEng{srcMgr};
  std::unique_ptr<ASTContext> ctxt{ASTContext::create(srcMgr, diagEng)};

  SourceLoc beg, mid, end;

  Expr *unresDeclRefExpr;
  Expr *unresMembRefExpr;
  Expr *declRefExpr;
  Expr *discardExpr;
  Expr *intLitExpr;
  Expr *fltLitExpr;
  Expr *boolLitExpr;
  Expr *nullLitExpr;
  Expr *implicitMaybeConvExpr;
  Expr *destructuredTupleEltExpr;
  Expr *loadExpr;
  Expr *errorExpr;
  Expr *castExpr;
  Expr *tupleEltExpr;
  Expr *tupleExpr;
  Expr *parenExpr;
  Expr *callExpr;
  Expr *condExpr;
  Expr *forceUnwrapExpr;
  Expr *binaryExpr;
  Expr *unaryExpr;
};
} // namespace

TEST_F(ExprTest, rtti) {
  EXPECT_TRUE(isa<UnresolvedDeclRefExpr>(unresDeclRefExpr));
  EXPECT_TRUE(isa<UnresolvedExpr>(unresDeclRefExpr));

  EXPECT_TRUE(isa<UnresolvedMemberRefExpr>(unresMembRefExpr));
  EXPECT_TRUE(isa<UnresolvedExpr>(unresMembRefExpr));

  EXPECT_TRUE(isa<DeclRefExpr>(declRefExpr));

  EXPECT_TRUE(isa<DiscardExpr>(discardExpr));

  EXPECT_TRUE(isa<IntegerLiteralExpr>(intLitExpr));
  EXPECT_TRUE(isa<AnyLiteralExpr>(intLitExpr));

  EXPECT_TRUE(isa<FloatLiteralExpr>(fltLitExpr));
  EXPECT_TRUE(isa<AnyLiteralExpr>(fltLitExpr));

  EXPECT_TRUE(isa<BooleanLiteralExpr>(boolLitExpr));
  EXPECT_TRUE(isa<AnyLiteralExpr>(boolLitExpr));

  EXPECT_TRUE(isa<NullLiteralExpr>(nullLitExpr));
  EXPECT_TRUE(isa<AnyLiteralExpr>(nullLitExpr));

  EXPECT_TRUE(isa<ImplicitMaybeConversionExpr>(implicitMaybeConvExpr));
  EXPECT_TRUE(isa<ImplicitConversionExpr>(implicitMaybeConvExpr));

  Expr *mtire = new (*ctxt) MutToImmutReferenceExpr(nullptr);
  EXPECT_TRUE(isa<MutToImmutReferenceExpr>(mtire));
  EXPECT_TRUE(isa<ImplicitConversionExpr>(mtire));

  Expr *dte = new (*ctxt) DestructuredTupleExpr(nullptr, nullptr);
  EXPECT_TRUE(isa<DestructuredTupleExpr>(dte));
  EXPECT_TRUE(isa<ImplicitConversionExpr>(dte));

  EXPECT_TRUE(isa<DestructuredTupleElementExpr>(destructuredTupleEltExpr));

  EXPECT_TRUE(isa<LoadExpr>(loadExpr));

  EXPECT_TRUE(isa<ErrorExpr>(errorExpr));

  EXPECT_TRUE(isa<CastExpr>(castExpr));

  EXPECT_TRUE(isa<TupleElementExpr>(tupleEltExpr));

  EXPECT_TRUE(isa<TupleExpr>(tupleExpr));

  EXPECT_TRUE(isa<ParenExpr>(parenExpr));

  EXPECT_TRUE(isa<CallExpr>(callExpr));

  EXPECT_TRUE(isa<ConditionalExpr>(condExpr));

  EXPECT_TRUE(isa<ForceUnwrapExpr>(forceUnwrapExpr));

  EXPECT_TRUE(isa<BinaryExpr>(binaryExpr));

  EXPECT_TRUE(isa<UnaryExpr>(unaryExpr));
}

TEST_F(ExprTest, IntegerLiteralExpr_getValue) {
  IntegerLiteralExpr *expr = new (*ctxt) IntegerLiteralExpr("300", {});
  expr->setType(ctxt->i8Type);
  EXPECT_EQ(expr->getValue().toString(10, true), "44");
  expr->setType(ctxt->u8Type);
  EXPECT_EQ(expr->getValue().toString(10, false), "44");
  expr->setType(ctxt->i16Type);
  EXPECT_EQ(expr->getValue().toString(10, true), "300");

  expr = new (*ctxt) IntegerLiteralExpr("200", {});
  expr->setType(ctxt->i8Type);
  EXPECT_EQ(expr->getValue().toString(10, true), "-56");
  expr->setType(ctxt->u8Type);
  EXPECT_EQ(expr->getValue().toString(10, false), "200");
  expr->setType(ctxt->i16Type);
  EXPECT_EQ(expr->getValue().toString(10, true), "200");
}

TEST_F(ExprTest, FloatLiteralExpr_getValue) {
  FloatLiteralExpr *expr = new (*ctxt) FloatLiteralExpr("3.14", {});
  expr->setType(ctxt->f32Type);
  SmallVector<char, 8> strVec;
  expr->getValue().toString(strVec);
  EXPECT_EQ(StringRef(strVec.data(), 4), "3.14");
  strVec.clear();
  expr->setType(ctxt->f64Type);
  expr->getValue().toString(strVec);
  EXPECT_EQ(StringRef(strVec.data(), 4), "3.14");
}

TEST_F(ExprTest, UnresolvedExprs) {
  // Create another ASTContext in which we'll allocate a few unresolved expr so
  // we can check that they're properly allocated using
  // ArenaKind::UnresolvedExpr.
  auto ctxt = ASTContext::create(srcMgr, diagEng);
  ASSERT_EQ(ctxt->getMemoryUsed(ArenaKind::UnresolvedExpr), 0);
  ASSERT_EQ(ctxt->getMemoryUsed(ArenaKind::TypeVariableEnvironment), 0);
  size_t basePerma = ctxt->getMemoryUsed(ArenaKind::Permanent);
  new (*ctxt) UnresolvedDeclRefExpr({}, {});
  new (*ctxt) UnresolvedMemberRefExpr(nullptr, {}, false, {}, {});
  EXPECT_NE(ctxt->getMemoryUsed(ArenaKind::UnresolvedExpr), 0);
  EXPECT_EQ(ctxt->getMemoryUsed(ArenaKind::Permanent), basePerma);
  EXPECT_EQ(ctxt->getMemoryUsed(ArenaKind::TypeVariableEnvironment), 0);
}

TEST_F(ExprTest, getSourceRange) {
  // UnresolvedDeclRefExpr
  EXPECT_EQ(beg, unresDeclRefExpr->getBegLoc());
  EXPECT_EQ(beg, unresDeclRefExpr->getLoc());
  EXPECT_EQ(beg, unresDeclRefExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), unresDeclRefExpr->getSourceRange());

  // UnresolvedDotExpr
  EXPECT_EQ(beg, unresMembRefExpr->getBegLoc());
  EXPECT_EQ(end, unresMembRefExpr->getEndLoc());
  EXPECT_EQ(mid, unresMembRefExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), unresMembRefExpr->getSourceRange());

  // DeclRefExpr
  EXPECT_EQ(beg, declRefExpr->getBegLoc());
  EXPECT_EQ(beg, declRefExpr->getLoc());
  EXPECT_EQ(beg, declRefExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), declRefExpr->getSourceRange());

  // DiscardExpr
  EXPECT_EQ(beg, discardExpr->getBegLoc());
  EXPECT_EQ(beg, discardExpr->getLoc());
  EXPECT_EQ(beg, discardExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), discardExpr->getSourceRange());

  // IntegerLiteralExpr
  EXPECT_EQ(beg, intLitExpr->getBegLoc());
  EXPECT_EQ(beg, intLitExpr->getLoc());
  EXPECT_EQ(beg, intLitExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), intLitExpr->getSourceRange());

  // FloatLiteralExpr
  EXPECT_EQ(beg, fltLitExpr->getBegLoc());
  EXPECT_EQ(beg, fltLitExpr->getLoc());
  EXPECT_EQ(beg, fltLitExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), fltLitExpr->getSourceRange());

  // BooleanLiteralExpr
  EXPECT_EQ(beg, boolLitExpr->getBegLoc());
  EXPECT_EQ(beg, boolLitExpr->getLoc());
  EXPECT_EQ(beg, boolLitExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), boolLitExpr->getSourceRange());

  // NullLiteralExpr
  EXPECT_EQ(beg, nullLitExpr->getBegLoc());
  EXPECT_EQ(beg, nullLitExpr->getLoc());
  EXPECT_EQ(beg, nullLitExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), nullLitExpr->getSourceRange());

  // ImplicitMaybeConversionExpr
  EXPECT_EQ(beg, implicitMaybeConvExpr->getBegLoc());
  EXPECT_EQ(beg, implicitMaybeConvExpr->getLoc());
  EXPECT_EQ(beg, implicitMaybeConvExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), implicitMaybeConvExpr->getSourceRange());

  // DestructuredTupleElementExpr
  EXPECT_EQ(beg, destructuredTupleEltExpr->getBegLoc());
  EXPECT_EQ(beg, destructuredTupleEltExpr->getLoc());
  EXPECT_EQ(end, destructuredTupleEltExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), destructuredTupleEltExpr->getSourceRange());

  // LoadExpr
  EXPECT_EQ(beg, loadExpr->getBegLoc());
  EXPECT_EQ(beg, loadExpr->getLoc());
  EXPECT_EQ(beg, loadExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, beg), loadExpr->getSourceRange());

  // ErrorExpr
  EXPECT_EQ(beg, errorExpr->getBegLoc());
  EXPECT_EQ(beg, errorExpr->getLoc());
  EXPECT_EQ(end, errorExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), errorExpr->getSourceRange());

  // CastExpr
  EXPECT_EQ(beg, castExpr->getBegLoc());
  EXPECT_EQ(end, castExpr->getEndLoc());
  EXPECT_EQ(mid, castExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), castExpr->getSourceRange());

  // TupleElementExpr
  EXPECT_EQ(beg, tupleEltExpr->getBegLoc());
  EXPECT_EQ(end, tupleEltExpr->getEndLoc());
  EXPECT_EQ(mid, tupleEltExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), tupleEltExpr->getSourceRange());

  // TupleExpr
  EXPECT_EQ(beg, tupleExpr->getBegLoc());
  EXPECT_EQ(beg, tupleExpr->getLoc());
  EXPECT_EQ(end, tupleExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), tupleExpr->getSourceRange());

  // ParenExpr
  EXPECT_EQ(beg, parenExpr->getBegLoc());
  EXPECT_EQ(end, parenExpr->getEndLoc());
  EXPECT_EQ(mid, parenExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), parenExpr->getSourceRange());

  // CallExpr
  EXPECT_EQ(beg, callExpr->getBegLoc());
  EXPECT_EQ(beg, callExpr->getLoc());
  EXPECT_EQ(end, callExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), callExpr->getSourceRange());

  // ConditionalExpr
  EXPECT_EQ(beg, condExpr->getBegLoc());
  EXPECT_EQ(mid, condExpr->getLoc());
  EXPECT_EQ(end, condExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), condExpr->getSourceRange());

  // ForceUnwrapExpr
  EXPECT_EQ(beg, forceUnwrapExpr->getBegLoc());
  EXPECT_EQ(beg, forceUnwrapExpr->getLoc());
  EXPECT_EQ(end, forceUnwrapExpr->getEndLoc());
  EXPECT_EQ(SourceRange(beg, end), forceUnwrapExpr->getSourceRange());

  // BinaryExpr
  EXPECT_EQ(beg, binaryExpr->getBegLoc());
  EXPECT_EQ(end, binaryExpr->getEndLoc());
  EXPECT_EQ(mid, binaryExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), binaryExpr->getSourceRange());

  // UnaryExpr
  EXPECT_EQ(beg, unaryExpr->getBegLoc());
  EXPECT_EQ(end, unaryExpr->getEndLoc());
  EXPECT_EQ(beg, unaryExpr->getLoc());
  EXPECT_EQ(SourceRange(beg, end), unaryExpr->getSourceRange());
}