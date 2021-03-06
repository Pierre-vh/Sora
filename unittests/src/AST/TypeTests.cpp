//===--- TypeTests.cpp ------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/AST/ASTContext.hpp"
#include "Sora/AST/TypeVariableEnvironment.hpp"
#include "Sora/AST/Types.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "Sora/Diagnostics/DiagnosticEngine.hpp"
#include "llvm/ADT/Triple.h"
#include "gtest/gtest.h"

using namespace sora;

namespace {
class TypeTest : public ::testing::Test {
protected:
  TypeTest() {
    env = std::make_unique<TypeVariableEnvironment>(*ctxt);

    refType = ReferenceType::get(ctxt->i32Type, false);
    maybeType = MaybeType::get(ctxt->i32Type);
    lvalueType = LValueType::get(ctxt->i32Type);
    tupleType = TupleType::getEmpty(*ctxt);
    tyVar = TypeVariableType::createGeneralTypeVariable(*env, 0);
    fnType = FunctionType::get({}, refType);
  }

  IntegerType *getSignedInt(IntegerWidth width) {
    return IntegerType::getSigned(*ctxt, width);
  }

  IntegerType *getUnsignedInt(IntegerWidth width) {
    return IntegerType::getUnsigned(*ctxt, width);
  }

  SourceManager srcMgr;
  DiagnosticEngine diagEng{srcMgr};
  std::unique_ptr<ASTContext> ctxt{ASTContext::create(srcMgr, diagEng)};

  std::unique_ptr<TypeVariableEnvironment> env;

  Type refType;
  Type maybeType;
  Type lvalueType;
  Type tupleType;
  Type tyVar;
  Type fnType;
};
} // namespace

TEST_F(TypeTest, rtti) {
  EXPECT_TRUE(ctxt->i32Type->is<IntegerType>());
  EXPECT_TRUE(ctxt->i32Type->is<BuiltinType>());

  EXPECT_TRUE(ctxt->f32Type->is<FloatType>());
  EXPECT_TRUE(ctxt->f32Type->is<BuiltinType>());

  EXPECT_TRUE(ctxt->voidType->is<VoidType>());
  EXPECT_TRUE(ctxt->voidType->is<BuiltinType>());

  EXPECT_TRUE(ctxt->boolType->is<BoolType>());
  EXPECT_TRUE(ctxt->boolType->is<BuiltinType>());

  EXPECT_TRUE(ctxt->errorType->is<ErrorType>());

  EXPECT_TRUE(refType->is<ReferenceType>());

  EXPECT_TRUE(maybeType->is<MaybeType>());

  EXPECT_TRUE(lvalueType->is<LValueType>());

  EXPECT_TRUE(tupleType->is<TupleType>());

  EXPECT_TRUE(tyVar->is<TypeVariableType>());

  EXPECT_TRUE(fnType->is<FunctionType>());
}

TEST_F(TypeTest, typeProperties) {
  EXPECT_FALSE(ctxt->errorType->hasTypeVariable());
  EXPECT_TRUE(ctxt->errorType->hasErrorType());

  EXPECT_TRUE(tyVar->hasTypeVariable());
  EXPECT_FALSE(tyVar->hasErrorType());
}

TEST_F(TypeTest, ReferenceTypeWithoutMut) {
  ReferenceType *immut = ReferenceType::get(ctxt->i32Type, false);
  ReferenceType *mut = ReferenceType::get(ctxt->i32Type, true);

  EXPECT_EQ(immut, immut->withoutMut());
  EXPECT_EQ(immut, mut->withoutMut());
}

TEST_F(TypeTest, TupleType) {
  Type i32 = ctxt->i32Type;
  Type emptyTuple = TupleType::getEmpty(*ctxt);
  EXPECT_EQ(TupleType::get(*ctxt, {}).getPtr(), emptyTuple.getPtr());
  EXPECT_EQ(TupleType::get(*ctxt, {i32}).getPtr(), i32.getPtr());
  EXPECT_EQ(TupleType::get(*ctxt, {i32, i32}).getPtr(),
            TupleType::get(*ctxt, {i32, i32}).getPtr());
}

// Test for the propagation of TypeProperties on "simple" wrapper types:
// LValueType, ReferenceType, MaybeType
TEST_F(TypeTest, simpleTypePropertiesPropagation) {
#define CHECK(CREATE, HAS_TV, HAS_ERR, HAS_NULL)                               \
  {                                                                            \
    auto ty = CREATE;                                                          \
    EXPECT_EQ(ty->hasTypeVariable(), HAS_TV);                                  \
    EXPECT_EQ(ty->hasErrorType(), HAS_ERR);                                    \
  }

  // i32: no properties set
  CHECK(LValueType::get(ctxt->i32Type), false, false, false);
  CHECK(ReferenceType::get(ctxt->i32Type, false), false, false, false);
  CHECK(MaybeType::get(ctxt->i32Type), false, false, false);

  // TypeVariableType
  ASSERT_TRUE(tyVar->hasTypeVariable());
  CHECK(LValueType::get(tyVar), true, false, false);
  CHECK(ReferenceType::get(tyVar, false), true, false, false);
  CHECK(MaybeType::get(tyVar), true, false, false);

  // ErrorType
  ASSERT_TRUE(ctxt->errorType->hasErrorType());
  CHECK(LValueType::get(ctxt->errorType), false, true, false);
  CHECK(ReferenceType::get(ctxt->errorType, false), false, true, false);
  CHECK(MaybeType::get(ctxt->errorType), false, true, false);

#undef CHECK
}

TEST_F(TypeTest, ASTContextSingletons) {
  EXPECT_EQ(ctxt->i8Type.getPtr(), getSignedInt(IntegerWidth::fixed(8)));
  EXPECT_EQ(ctxt->i16Type.getPtr(), getSignedInt(IntegerWidth::fixed(16)));
  EXPECT_EQ(ctxt->i32Type.getPtr(), getSignedInt(IntegerWidth::fixed(32)));
  EXPECT_EQ(ctxt->i64Type.getPtr(), getSignedInt(IntegerWidth::fixed(64)));
  EXPECT_EQ(ctxt->isizeType.getPtr(),
            getSignedInt(IntegerWidth::pointer(ctxt->getTargetTriple())));

  EXPECT_EQ(ctxt->u8Type.getPtr(), getUnsignedInt(IntegerWidth::fixed(8)));
  EXPECT_EQ(ctxt->u16Type.getPtr(), getUnsignedInt(IntegerWidth::fixed(16)));
  EXPECT_EQ(ctxt->u32Type.getPtr(), getUnsignedInt(IntegerWidth::fixed(32)));
  EXPECT_EQ(ctxt->u64Type.getPtr(), getUnsignedInt(IntegerWidth::fixed(64)));
  EXPECT_EQ(ctxt->usizeType.getPtr(),
            getUnsignedInt(IntegerWidth::pointer(ctxt->getTargetTriple())));

  EXPECT_EQ(ctxt->f32Type.getPtr(), FloatType::get(*ctxt, FloatKind::IEEE32));
  EXPECT_EQ(ctxt->f64Type.getPtr(), FloatType::get(*ctxt, FloatKind::IEEE64));
}

TEST_F(TypeTest, integerTypes) {
#define CHECK(MEMBER, WIDTH, ISSIGNED)                                         \
  EXPECT_EQ(ctxt->MEMBER->castTo<IntegerType>()->getWidth(), WIDTH);           \
  EXPECT_EQ(ctxt->MEMBER->castTo<IntegerType>()->isSigned(), ISSIGNED)

  CHECK(i8Type, IntegerWidth::fixed(8), true);
  CHECK(i16Type, IntegerWidth::fixed(16), true);
  CHECK(i32Type, IntegerWidth::fixed(32), true);
  CHECK(i64Type, IntegerWidth::fixed(64), true);
  CHECK(isizeType, IntegerWidth::pointer(ctxt->getTargetTriple()), true);

  CHECK(u8Type, IntegerWidth::fixed(8), false);
  CHECK(u16Type, IntegerWidth::fixed(16), false);
  CHECK(u32Type, IntegerWidth::fixed(32), false);
  CHECK(u64Type, IntegerWidth::fixed(64), false);
  CHECK(usizeType, IntegerWidth::pointer(ctxt->getTargetTriple()), false);

#undef CHECK
}

TEST_F(TypeTest, tupleTypes) {
  ASSERT_EQ(TupleType::getEmpty(*ctxt), TupleType::get(*ctxt, {}).getPtr());
  ASSERT_EQ(ctxt->f32Type.getPtr(),
            TupleType::get(*ctxt, {ctxt->f32Type}).getPtr());
}

TEST_F(TypeTest, canonicalTypes_alwaysCanonicalTypes) {
#define CHECK_ALWAYS_CANONICAL(T)                                              \
  {                                                                            \
    auto can = T->getCanonicalType();                                          \
    EXPECT_EQ(can.getPtr(), T.getPtr());                                       \
    ASSERT_TRUE(can->isCanonical());                                           \
    ASSERT_TRUE(T->isCanonical());                                             \
  }
  CHECK_ALWAYS_CANONICAL(ctxt->f32Type);
  CHECK_ALWAYS_CANONICAL(ctxt->f64Type);
  CHECK_ALWAYS_CANONICAL(ctxt->i8Type);
  CHECK_ALWAYS_CANONICAL(ctxt->i16Type);
  CHECK_ALWAYS_CANONICAL(ctxt->i32Type);
  CHECK_ALWAYS_CANONICAL(ctxt->i64Type);
  CHECK_ALWAYS_CANONICAL(ctxt->isizeType);
  CHECK_ALWAYS_CANONICAL(ctxt->u8Type);
  CHECK_ALWAYS_CANONICAL(ctxt->u16Type);
  CHECK_ALWAYS_CANONICAL(ctxt->u32Type);
  CHECK_ALWAYS_CANONICAL(ctxt->u64Type);
  CHECK_ALWAYS_CANONICAL(ctxt->usizeType);
  CHECK_ALWAYS_CANONICAL(ctxt->voidType);
  CHECK_ALWAYS_CANONICAL(ctxt->boolType);
  CHECK_ALWAYS_CANONICAL(ctxt->errorType);
  Type tyVar = TypeVariableType::createGeneralTypeVariable(*env, 0);
  CHECK_ALWAYS_CANONICAL(tyVar);
#undef CHECK_ALWAYS_CANONICAL
}

TEST_F(TypeTest, canonicalTypes) {
  auto emptyTuple = TupleType::getEmpty(*ctxt);
  auto voidTy = ctxt->voidType;

#define CHECK(NON_CAN, CAN)                                                    \
  EXPECT_EQ(NON_CAN->getCanonicalType().getPtr(), CAN);

  CHECK(MaybeType::get(emptyTuple), MaybeType::get(voidTy));
  CHECK(LValueType::get(emptyTuple), LValueType::get(voidTy));

  CHECK(ReferenceType::get(emptyTuple, true), ReferenceType::get(voidTy, true));
  CHECK(ReferenceType::get(emptyTuple, false),
        ReferenceType::get(voidTy, false));

  CHECK(TupleType::getEmpty(*ctxt), ctxt->voidType.getPtr());
  CHECK(TupleType::get(*ctxt, {emptyTuple, emptyTuple}),
        TupleType::get(*ctxt, {ctxt->voidType, ctxt->voidType}).getPtr());

  CHECK(FunctionType::get({}, emptyTuple), FunctionType::get({}, voidTy));
  CHECK(FunctionType::get({emptyTuple}, emptyTuple),
        FunctionType::get({voidTy}, voidTy));
  CHECK(FunctionType::get({emptyTuple, emptyTuple}, emptyTuple),
        FunctionType::get({voidTy, voidTy}, voidTy));
#undef CHECK
}

TEST_F(TypeTest, printingTest_simple) {
  TypePrintOptions opts = TypePrintOptions::forDebug();
#define CHECK(T, STR) EXPECT_EQ(T.getString(opts), STR)
  CHECK(ctxt->f32Type, "f32");
  CHECK(ctxt->f64Type, "f64");
  CHECK(ctxt->i8Type, "i8");
  CHECK(ctxt->i16Type, "i16");
  CHECK(ctxt->i32Type, "i32");
  CHECK(ctxt->i64Type, "i64");
  CHECK(ctxt->isizeType, "isize");
  CHECK(ctxt->u8Type, "u8");
  CHECK(ctxt->u16Type, "u16");
  CHECK(ctxt->u32Type, "u32");
  CHECK(ctxt->u64Type, "u64");
  CHECK(ctxt->usizeType, "usize");
  CHECK(ctxt->voidType, "void");
  CHECK(ctxt->boolType, "bool");
  CHECK(ctxt->errorType, "<error_type>");
  CHECK(tyVar, "$T0");
  CHECK(Type(nullptr), "<null_type>");
#undef CHECK
}

TEST_F(TypeTest, printingTest_lvalues) {
  Type lvalue = LValueType::get(ctxt->f32Type);
  EXPECT_EQ(lvalue->getString(TypePrintOptions::forDebug()), "@lvalue f32");
  EXPECT_EQ(lvalue->getString(TypePrintOptions::forDiagnostics()), "f32");
}

TEST_F(TypeTest, printingTest_references) {
  Type mutRef = ReferenceType::get(ctxt->f32Type, true);
  Type ref = ReferenceType::get(ctxt->f32Type, false);

  EXPECT_EQ(mutRef->getString(TypePrintOptions::forDebug()), "&mut f32");
  EXPECT_EQ(mutRef->getString(TypePrintOptions::forDiagnostics()), "&mut f32");

  EXPECT_EQ(ref->getString(TypePrintOptions::forDebug()), "&f32");
  EXPECT_EQ(ref->getString(TypePrintOptions::forDiagnostics()), "&f32");
}

TEST_F(TypeTest, printingTest_maybe) {
  Type maybe = MaybeType::get(ctxt->f32Type);
  EXPECT_EQ(maybe->getString(TypePrintOptions::forDebug()), "maybe f32");
  EXPECT_EQ(maybe->getString(TypePrintOptions::forDiagnostics()), "maybe f32");
}

TEST_F(TypeTest, printingTest_tuple) {
  Type t0 = TupleType::getEmpty(*ctxt);
  Type t2 = TupleType::get(*ctxt, {ctxt->f32Type, ctxt->f64Type});
  Type t3 =
      TupleType::get(*ctxt, {ctxt->f32Type, ctxt->f64Type, ctxt->isizeType});

  EXPECT_EQ(t0->getString(TypePrintOptions::forDebug()), "()");
  EXPECT_EQ(t0->getString(TypePrintOptions::forDiagnostics()), "()");

  EXPECT_EQ(t2->getString(TypePrintOptions::forDebug()), "(f32, f64)");
  EXPECT_EQ(t2->getString(TypePrintOptions::forDiagnostics()), "(f32, f64)");

  EXPECT_EQ(t3->getString(TypePrintOptions::forDebug()), "(f32, f64, isize)");
  EXPECT_EQ(t3->getString(TypePrintOptions::forDiagnostics()),
            "(f32, f64, isize)");
}

TEST_F(TypeTest, printingTest_func) {
  Type f0 = FunctionType::get({}, ctxt->voidType);
  Type f1 = FunctionType::get({ctxt->f32Type, ctxt->f64Type}, ctxt->voidType);
  Type f2 = FunctionType::get({ctxt->f32Type, ctxt->f64Type, ctxt->isizeType},
                              ctxt->voidType);

  EXPECT_EQ(f0->getString(TypePrintOptions::forDebug()), "() -> void");
  EXPECT_EQ(f0->getString(TypePrintOptions::forDiagnostics()), "() -> void");

  EXPECT_EQ(f1->getString(TypePrintOptions::forDebug()), "(f32, f64) -> void");
  EXPECT_EQ(f1->getString(TypePrintOptions::forDiagnostics()),
            "(f32, f64) -> void");

  EXPECT_EQ(f2->getString(TypePrintOptions::forDebug()),
            "(f32, f64, isize) -> void");
  EXPECT_EQ(f2->getString(TypePrintOptions::forDiagnostics()),
            "(f32, f64, isize) -> void");
}

TEST_F(TypeTest, printingTest_typeVariables) {
  auto *generalTyVar = TypeVariableType::createGeneralTypeVariable(*env, 0);
  EXPECT_EQ(generalTyVar->getString(TypePrintOptions::forDebug()), "$T0");
  EXPECT_EQ(generalTyVar->getString(TypePrintOptions::forDiagnostics()), "_");
  env->bind(generalTyVar, ctxt->voidType);
  EXPECT_EQ(generalTyVar->getString(TypePrintOptions::forDebug()), "$T0(void)");
  TypePrintOptions tpo = TypePrintOptions::forDiagnostics();
  EXPECT_EQ(generalTyVar->getString(tpo), "void");
  tpo.printBoundTypeVariablesAsBinding = false;
  EXPECT_EQ(generalTyVar->getString(tpo), "_");

  auto *intTyVar = TypeVariableType::createIntegerTypeVariable(*env, 0);
  EXPECT_EQ(intTyVar->getString(TypePrintOptions::forDebug()), "$I0");
  EXPECT_EQ(intTyVar->getString(TypePrintOptions::forDiagnostics()), "_");
  env->bind(intTyVar, ctxt->i32Type);
  EXPECT_EQ(intTyVar->getString(TypePrintOptions::forDebug()), "$I0(i32)");
  tpo = TypePrintOptions::forDiagnostics();
  EXPECT_EQ(intTyVar->getString(tpo), "i32");
  tpo.printBoundTypeVariablesAsBinding = false;
  EXPECT_EQ(intTyVar->getString(tpo), "_");

  auto *floatTyVar = TypeVariableType::createFloatTypeVariable(*env, 0);
  EXPECT_EQ(floatTyVar->getString(TypePrintOptions::forDebug()), "$F0");
  EXPECT_EQ(floatTyVar->getString(TypePrintOptions::forDiagnostics()), "_");
  env->bind(floatTyVar, ctxt->f64Type);
  EXPECT_EQ(floatTyVar->getString(TypePrintOptions::forDebug()), "$F0(f64)");
  tpo = TypePrintOptions::forDiagnostics();
  EXPECT_EQ(floatTyVar->getString(tpo), "f64");
  tpo.printBoundTypeVariablesAsBinding = false;
  EXPECT_EQ(floatTyVar->getString(tpo), "_");
}

TEST_F(TypeTest, rebuildType) {
  // Create a fairly complex type, and swap all i8 in it for u8s and make all
  // references immutable.
  SmallVector<Type, 4> args;
  args.push_back(ctxt->i8Type);
  args.push_back(ReferenceType::get(args.back(), true));
  args.push_back(ReferenceType::get(args.back(), true));
  args.push_back(ReferenceType::get(args.back(), true));
  args.push_back(ctxt->voidType);

  SmallVector<Type, 4> tupleElt;
  tupleElt.push_back(ctxt->i8Type);
  tupleElt.push_back(ReferenceType::get(tupleElt.back(), true));
  tupleElt.push_back(ctxt->voidType);

  FunctionType *fn = FunctionType::get(args, TupleType::get(*ctxt, tupleElt));
  Type finalType = LValueType::get(fn);

  ASSERT_EQ(finalType->getString(TypePrintOptions::forDebug()),
            "@lvalue (i8, &mut i8, &mut &mut i8, &mut &mut &mut i8, void) -> "
            "(i8, &mut i8, void)");

  Type rebuilt = finalType->rebuildType([&](Type type) -> Type {
    if (type->getCanonicalType() == ctxt->i8Type)
      return ctxt->u8Type;
    if (auto *ref = type->getAs<ReferenceType>())
      return ReferenceType::get(ref->getPointeeType(), false);
    return nullptr;
  });

  ASSERT_EQ(rebuilt->getString(TypePrintOptions::forDebug()),
            "@lvalue (u8, &u8, &&u8, &&&u8, void) -> (u8, &u8, void)");
}