//===--- DeclContextTests.cpp -----------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/Decl.hpp"
#include "Sora/AST/DeclContext.hpp"
#include "Sora/AST/SourceFile.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "Sora/Diagnostics/DiagnosticEngine.hpp"
#include "gtest/gtest.h"

using namespace sora;

namespace {
class DeclContextTest : public ::testing::Test {
protected:
  DeclContextTest()
      : sf(SourceFile::create(*ctxt, {}, nullptr)),
        func(new (*ctxt) FuncDecl(sf, {}, {}, {},
                                  ParamList::createEmpty(*ctxt, {}, {}), {})) {}

  SourceManager srcMgr;
  DiagnosticEngine diagEng{srcMgr};
  std::unique_ptr<ASTContext> ctxt{ASTContext::create(srcMgr, diagEng)};

  SourceFile *sf;
  FuncDecl *func;
};
} // namespace

TEST_F(DeclContextTest, isLocalContext) {
  DeclContext *dc = func;
  EXPECT_TRUE(dc->isLocalContext());
  dc = sf;
  EXPECT_FALSE(dc->isLocalContext());
}

TEST_F(DeclContextTest, rtti) {
  EXPECT_TRUE(isa<DeclContext>(sf));
  EXPECT_TRUE(isa<DeclContext>(func));
}