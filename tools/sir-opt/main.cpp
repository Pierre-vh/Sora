//===--- main.cpp - Sora IR Opt Tool Entry Point ----------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2020 Pierre van Houtryve
//===----------------------------------------------------------------------===//
// This is a MLIR opt-like tool for Sora IR, which can be used to round-trip the
// IR and run passes.
//===----------------------------------------------------------------------===//

#include "Sora/EntryPoints.hpp"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Support/MlirOptMain.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace sora;

static llvm::cl::opt<std::string> inputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input file>"),
                                                llvm::cl::init("-"));

static llvm::cl::opt<std::string>
    outputFilename("o", llvm::cl::desc("Output filename"),
                   llvm::cl::value_desc("filename"), llvm::cl::init("-"));

static llvm::cl::opt<bool> splitInputFile(
    "split-input-file",
    llvm::cl::desc("Split the input file into pieces and process each "
                   "chunk independently"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> verifyDiagnostics(
    "verify-diagnostics",
    llvm::cl::desc("Check that emitted diagnostics match "
                   "expected-* lines on the corresponding line"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> verifyPasses(
    "verify-each",
    llvm::cl::desc("Run the verifier after each transformation pass"),
    llvm::cl::init(true));

namespace mlir {
namespace sora {
// Passes are added as-needed.
void registerSupportedPasses() {
#define GEN_PASS_REGISTRATION_CSE
#define GEN_PASS_REGISTRATION_Canonicalizer
#define GEN_PASS_REGISTRATION_Inliner
#define GEN_PASS_REGISTRATION_PrintCFG
#define GEN_PASS_REGISTRATION_PrintOp
#define GEN_PASS_REGISTRATION_PrintOpStats
#define GEN_PASS_REGISTRATION_SCCP
#define GEN_PASS_REGISTRATION_StripDebugInfo
#define GEN_PASS_REGISTRATION_SymbolDCE
#include "mlir/Transforms/Passes.h.inc"
#define GEN_PASS_REGISTRATION
#include "mlir/Dialect/StandardOps/Transforms/Passes.h.inc"
}
} // namespace sora
} // namespace mlir

int main(int argc, char **argv) {
  llvm::InitLLVM initLLVM(argc, argv);

  // Register Sora's MLIR Dialects.
  registerMLIRDialects();

  // Register all command line options.
  mlir::registerPassManagerCLOptions();
  mlir::registerAsmPrinterCLOptions();
  mlir::registerMLIRContextCLOptions();

  // Register the passes supported by this tool.
  mlir::sora::registerSupportedPasses();

  mlir::PassPipelineCLParser passPipeline("", "Compiler passes to run");

  // Parse pass names in main to ensure static initialization completed.
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Sora IR (SIR) modular optimizer driver\n");

  std::string errorMessage;
  auto file = mlir::openInputFile(inputFilename, &errorMessage);
  if (!file) {
    llvm::errs() << errorMessage << "\n";
    return EXIT_FAILURE;
  }

  auto output = mlir::openOutputFile(outputFilename, &errorMessage);
  if (!output) {
    llvm::errs() << errorMessage << "\n";
    return EXIT_FAILURE;
  }

  if (failed(MlirOptMain(output->os(), std::move(file), passPipeline,
                         splitInputFile, verifyDiagnostics, verifyPasses,
                         /*allowUnregisteredDialects*/ false))) {
    return EXIT_FAILURE;
  }

  // Keep the output file if the invocation of MlirOptMain was successful.
  output->keep();
  return EXIT_SUCCESS;
}