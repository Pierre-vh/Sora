//===--- Dialect.cpp --------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/SIR/Dialect.hpp"

#include "Sora/Common/LLVM.hpp"
#include "Sora/SIR/Types.hpp"
#include "mlir/IR/Builders.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LogicalResult.h"

using namespace sora;
using namespace sora::sir;

//===----------------------------------------------------------------------===//
// Dialect
//===----------------------------------------------------------------------===//

SIRDialect::SIRDialect(mlir::MLIRContext *mlirCtxt)
    : mlir::Dialect("sir", mlirCtxt) {
  addOperations<
#define GET_OP_LIST
#include "Sora/SIR/Ops.cpp.inc"
      >();

  addTypes<MaybeType>();
  addTypes<ReferenceType>();
  addTypes<PointerType>();
  addTypes<VoidType>();
}

//===----------------------------------------------------------------------===//
// StaticCastOp
//===----------------------------------------------------------------------===//

/// FIXME: Are there really no invariants to verify?
static mlir::LogicalResult verify(StaticCastOp op) { return mlir::success(); }

//===----------------------------------------------------------------------===//
// AllocStackOp
//===----------------------------------------------------------------------===//

static mlir::LogicalResult verify(AllocStackOp op) {
  // No invariants to verify. This operation has to have a PointerType result
  // but that's already enforced in tablegen.
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// BitNotOp
//===----------------------------------------------------------------------===//

static void print(mlir::OpAsmPrinter &p, BitNotOp op) {
  // 'sir.bitnot' operand ':' type
  p << op.getOperationName();
  p << " : " << op.getType();
}

static mlir::ParseResult parseBitNotOp(mlir::OpAsmParser &parser,
                                       mlir::OperationState &result) {
  // 'sir.bitnot' operand ':' type
  mlir::OpAsmParser::OperandType operandType;
  if (parser.parseOperand(operandType))
    return mlir::failure();

  mlir::Type type;
  if (parser.parseColonType(type))
    return mlir::failure();
  result.addTypes(type);

  // Resolve the operand
  SmallVector<mlir::Value, 1> operand;
  if (parser.resolveOperand(operandType, type, operand))
    return mlir::failure();
  assert(operand.size() == 1 && "Expected a single result!");
  result.addOperands(operand[0]);

  return mlir::success();
}

static mlir::LogicalResult verify(BitNotOp op) {
  // Result type must be the same as the operand's type.
  return (op.getOperand().getType() == op.getType()) ? mlir::success()
                                                     : mlir::failure();
}

//===----------------------------------------------------------------------===//
// LoadOp
//===----------------------------------------------------------------------===//

static mlir::LogicalResult verify(LoadOp op) {
  // Invariants are already verified by the TypesMatchWith trait.
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// StoreOp
//===----------------------------------------------------------------------===//

static mlir::LogicalResult verify(StoreOp op) {
  // Invariants are already verified by the TypesMatchWith trait.
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// CreateTupleOp
//===----------------------------------------------------------------------===//

static void buildCreateTupleOp(mlir::OpBuilder &builder,
                               mlir::OperationState &result,
                               ArrayRef<mlir::Value> elts) {
  assert(elts.size() > 0 && "Cannot create empty tuples!");
  assert(elts.size() > 1 && "Cannot create single-element tuples");

  SmallVector<mlir::Type, 8> types;
  types.reserve(elts.size());

  for (const mlir::Value &elt : elts)
    types.push_back(elt.getType());

  mlir::TupleType tupleType = mlir::TupleType::get(types, builder.getContext());

  result.addOperands(elts);
  result.addTypes(tupleType);
}

static mlir::LogicalResult verify(CreateTupleOp op) {
  if (op.getNumOperands() <= 1)
    return op.emitOpError(
        "should have at least 2 operands - tuple of size <2 are not supported");

  mlir::TupleType tupleType = op.getType().dyn_cast<mlir::TupleType>();
  if (!tupleType)
    return op.emitOpError("result type should be a tuple type");

  if (tupleType.size() != op.getNumOperands())
    return op.emitOpError(
        "the number of operands and result tuple elements should be equal");

  for (size_t k = 0; k < op.getNumOperands(); ++k)
    if (op.getOperand(k).getType() != tupleType.getType(k))
      return op.emitOpError(
          "the type of the operand and result tuple element at index '" +
          std::to_string(k) + " is not equal");

  return mlir::success();
}

//===----------------------------------------------------------------------===//
// DestructureTupleOp
//===----------------------------------------------------------------------===//

static void buildDestructureTupleOp(mlir::OpBuilder &builder,
                                    mlir::OperationState &result,
                                    mlir::Value tuple) {
  mlir::TupleType tupleType = tuple.getType().dyn_cast<mlir::TupleType>();
  assert(tupleType && "The value must have a TupleType!");
  assert(tupleType.getTypes().size() > 1 && "Illegal tuple!");

  result.addOperands(tuple);
  result.addTypes(tupleType.getTypes());
}

static mlir::LogicalResult verify(DestructureTupleOp op) {
  mlir::TupleType tupleType =
      op.getOperand().getType().dyn_cast<mlir::TupleType>();
  if (!tupleType)
    return op.emitOpError("operand type should be a tuple type");

  ArrayRef<mlir::Type> results = op.getResultTypes();
  ArrayRef<mlir::Type> tupleElts = tupleType.getTypes();

  if (results.size() != tupleElts.size())
    return op.emitOpError(
        "the number of results and operand tuple elements should be equal");

  if (results.size() <= 1)
    return op.emitOpError("Only tuples of size >2 are allowed");

  for (size_t k = 0; k < results.size(); ++k)
    if (results[k] != tupleElts[k])
      return op.emitOpError(
          "the type of the result and tuple element at index '" +
          std::to_string(k) + " is not equal");

  return mlir::success();
}

//===----------------------------------------------------------------------===//
// BlockTerminatorOp
//===----------------------------------------------------------------------===//

/// FIXME: Are there really no invariants to verify?
static mlir::LogicalResult verify(BlockTerminatorOp op) {
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// BlockOp
//===----------------------------------------------------------------------===//

static void print(mlir::OpAsmPrinter &p, BlockOp op) {
  // 'sir.block' region
  p << op.getOperationName();
  p.printRegion(op.region(), /*printEntryBlockArgs*/ true,
                /*printBlockTerminators*/ false);
}

static mlir::ParseResult parseBlockOp(mlir::OpAsmParser &parser,
                                      mlir::OperationState &result) {
  // 'sir.block' region
  mlir::Region *region = result.addRegion();
  if (parser.parseRegion(*region, llvm::None, llvm::None))
    return mlir::failure();
  BlockOp::ensureTerminator(*region, parser.getBuilder(), result.location);
  return mlir::success();
}

static void buildBlockOp(mlir::OpBuilder &builder,
                         mlir::OperationState &result) {
  mlir::Region *region = result.addRegion();
  region->push_back(new mlir::Block());
  BlockOp::ensureTerminator(*region, builder, result.location);
}

/// FIXME: Are there really no invariants to verify?
static mlir::LogicalResult verify(BlockOp op) { return mlir::success(); }

//===----------------------------------------------------------------------===//
// VoidConstantOp
//===----------------------------------------------------------------------===//

static void print(mlir::OpAsmPrinter &p, VoidConstantOp op) {
  // 'sir.void_constant'
  p << op.getOperationName();
}

static mlir::ParseResult parseVoidConstantOp(mlir::OpAsmParser &parser,
                                             mlir::OperationState &result) {
  // 'sir.void_constant'
  result.addTypes(VoidType::get(result.getContext()));
  return mlir::success();
}

static mlir::LogicalResult verify(VoidConstantOp op) {
  // Invariants are already enforced through constraints in tablegen.
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// DefaultReturnOp
//===----------------------------------------------------------------------===//

static mlir::LogicalResult verify(DefaultReturnOp op) {
  return mlir::success();
}

//===----------------------------------------------------------------------===//
// TableGen'd Method Definitions
//===----------------------------------------------------------------------===//

using namespace ::mlir;
#define GET_OP_CLASSES
#include "Sora/SIR/Ops.cpp.inc"
