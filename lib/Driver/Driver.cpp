//===--- Driver.cpp ---------------------------------------------*- C++ -*-===//
// Part of the Sora project, licensed under the MIT license.
// See LICENSE.txt in the project root for license information.
//
// Copyright (c) 2019 Pierre van Houtryve
//===----------------------------------------------------------------------===//

#include "Sora/Driver/Driver.hpp"
#include "Sora/Common/DiagnosticEngine.hpp"
#include "Sora/Common/DiagnosticsDriver.hpp"
#include "Sora/Common/SourceManager.hpp"
#include "Sora/Driver/Options.hpp"
#include "llvm/ADT/ArrayRef.h"

using namespace sora;
using namespace llvm::opt;

Driver::Driver(DiagnosticEngine &driverDiags)
    : driverDiags(driverDiags), optTable(createSoraOptTable()) {}

InputArgList Driver::parseArgs(ArrayRef<const char *> args, bool &hadError) {
  hadError = false;
  unsigned missingArgIndex = 0, missingArgCount = 0;
  // Parse the arguments, set the argList.
  llvm::opt::InputArgList argList =
      optTable->ParseArgs(args, missingArgIndex, missingArgCount);
  // Check for unknown arguments & diagnose them
  for (const Arg *arg : argList.filtered(opt::OPT_UNKNOWN)) {
    hadError = true;
    diagnose(diag::unknown_arg, arg->getAsString(argList));
  }
  // Diagnose missing arguments
  if (missingArgCount) {
    diagnose(diag::missing_argv, argList.getArgString(missingArgIndex),
             missingArgCount);
    hadError = true;
  }
  return argList;
}

bool Driver::handleImmediateArgs(InputArgList &options) {
  // handle -h/-help
  if (options.hasArg(opt::OPT_help)) {
    optTable->PrintHelp(llvm::outs(), "sorac [options] <inputs>",
                        "Sora Compiler");
    return false;
  }
  return true;
}

/*
  TODO:
    - unit-test options stuff
    - class CompilerInstance (friend class Driver)
      - Optional<CompilerInstance> Driver::createCompilerInstance
        - Creates and set-up a compiler instance. returns false on error.
      - (unique_ptr)ASTContext, DiagnosticEngine, SourceManager, etc.
      - struct Options
        - bool dumpAST
        - bool dumpParse
        - bool dumpIR
      - enum class Kind { ParseOnly, Compile }
      - bool run() // return true if success, false otherwise.
          - doParse
          - doSema
          - doIRGen
          - doIROpt
          - doLLVMIRGen
          - doLLVMIROpt
          - doLLVMCodeGen
*/