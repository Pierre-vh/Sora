//===--- SIRGenPattern.cpp - Pattern SIR Generation -------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "SIRGen.hpp"

#include "Sora/AST/ASTVisitor.hpp"
#include "Sora/AST/Pattern.hpp"

using namespace sora;

//===- PatternGenerator ---------------------------------------------------===//
namespace {

/// The pattern generator works by visiting the pattern, and generating code for
/// every node. Most patterns don't even generate any code, in fact only the
/// VarPattern pattern generates something interesting - most of the work
/// happens when a value is provided.
/// When a value is provided (= the second arg isn't None), the pattern
/// generator will destructure that value to assign it the pattern elements.
class PatternGenerator
    : public SIRGeneratorBase,
      public PatternVisitor<PatternGenerator, void, Optional<mlir::Value>> {

public:
  using Base = PatternVisitor<PatternGenerator, void, Optional<mlir::Value>>;

  PatternGenerator(SIRGen &sirGen, mlir::OpBuilder &builder)
      : SIRGeneratorBase(sirGen), builder(builder) {}

  mlir::OpBuilder &builder;

  void visitVarPattern(VarPattern *pattern, Optional<mlir::Value> value);
  void visitDiscardPattern(DiscardPattern *pattern,
                           Optional<mlir::Value> value);
  void visitMutPattern(MutPattern *pattern, Optional<mlir::Value> value);
  void visitParenPattern(ParenPattern *pattern, Optional<mlir::Value> value);
  void visitTuplePattern(TuplePattern *pattern, Optional<mlir::Value> value);
  void visitTypedPattern(TypedPattern *pattern, Optional<mlir::Value> value);
  void visitMaybeValuePattern(MaybeValuePattern *pattern,
                              Optional<mlir::Value> value);
};

void PatternGenerator::visitVarPattern(VarPattern *pattern,
                                       Optional<mlir::Value> value) {
  mlir::Value address = sirGen.genVarDeclAlloc(builder, pattern->getVarDecl());
  if (value)
    builder.create<sir::StoreOp>(getNodeLoc(pattern), *value, address);
}

void PatternGenerator::visitDiscardPattern(DiscardPattern *,
                                           Optional<mlir::Value>) {
  // No-op, it just discards the value as the name implies.
}

void PatternGenerator::visitMutPattern(MutPattern *pattern,
                                       Optional<mlir::Value> value) {
  // This pattern is "transparent". Just emit its subpattern.
  visit(pattern->getSubPattern(), value);
}

void PatternGenerator::visitParenPattern(ParenPattern *pattern,
                                         Optional<mlir::Value> value) {
  // This pattern is "transparent". Just emit its subpattern.
  visit(pattern->getSubPattern(), value);
}

void PatternGenerator::visitTuplePattern(TuplePattern *pattern,
                                         Optional<mlir::Value> value) {
  // If this is an empty pattern, don't do anything, the value is just
  // discarded.
  if (pattern->isEmpty())
    return;

  // If we don't have an initial value, or if the pattern doesn't contain any
  // VarDecl, just generate the elements.
  /// This helps avoiding an extra useless sir.destructure_tuple in cases where
  /// the pattern is something like (_, _)
  if (!value || !pattern->hasVarPattern()) {
    for (Pattern *elt : pattern->getElements())
      visit(elt, None);
    return;
  }

  // If we have an initial value, generate a destructure_tuple for it, and
  // generate each element of the TuplePattern with the corresponding tuple
  // value as initial value.
  assert(value->getType().isa<mlir::TupleType>() && "Value is not a tuple!");

  auto destructureTupleOp =
      builder.create<sir::DestructureTupleOp>(getNodeLoc(pattern), *value);

  mlir::ResultRange destructureTupleElts = destructureTupleOp.getResults();
  ArrayRef<Pattern *> patternElts = pattern->getElements();

  assert(destructureTupleElts.size() == patternElts.size() &&
         "Illegal structure!");

  for (size_t k = 0; k < patternElts.size(); ++k)
    visit(patternElts[k], destructureTupleElts[k]);
}

void PatternGenerator::visitTypedPattern(TypedPattern *pattern,
                                         Optional<mlir::Value> value) {
  // This pattern is "transparent". Just emit its subpattern.
  visit(pattern->getSubPattern(), value);
}

void PatternGenerator::visitMaybeValuePattern(MaybeValuePattern *pattern,
                                              Optional<mlir::Value> value) {
  // These should only be present at the top level a LetDecl's pattern when it
  // is used as a condition, and need special handling.
  llvm_unreachable("Generation of MaybeValuePatterns through the "
                   "PatternGenerator is not supported!");
}

} // namespace

//===- SIRGen -------------------------------------------------------------===//

void SIRGen::genPattern(mlir::OpBuilder &builder, Pattern *pattern,
                        Optional<mlir::Value> value) {
  PatternGenerator(*this, builder).visit(pattern, value);
}

mlir::Location SIRGen::getNodeLoc(Pattern *pattern) {
  return mlir::OpaqueLoc::get(pattern, getLoc(pattern->getLoc()));
}