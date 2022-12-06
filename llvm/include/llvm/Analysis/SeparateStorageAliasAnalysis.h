//===- SeparateStorageAliasAnalysis.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
// Interface to the separate storage alias analysis pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_SEPARATESTORAGEALIASANALYSIS
#define LLVM_ANALYSIS_SEPARATESTORAGEALIASANALYSIS

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {
class AssumptionCache;
class DominatorTree;

/// A simple AA result that uses calls to the separate storage intrinsics.
class SeparateStorageAAResult : public AAResultBase {
public:
  SeparateStorageAAResult(AssumptionCache &AC, DominatorTree &DT);
  bool invalidate(Function &Fn, const PreservedAnalyses &PA,
                  FunctionAnalysisManager::Invalidator &Inv);
  AliasResult alias(const MemoryLocation &LocA, const MemoryLocation &LocB,
                    AAQueryInfo &AAQI, const Instruction *I);

private:
  AssumptionCache &AC;
  DominatorTree &DT;
};

/// Analysis pass providing a never-invalidated alias analysis result.
class SeparateStorageAA : public AnalysisInfoMixin<SeparateStorageAA> {
  friend AnalysisInfoMixin<SeparateStorageAA>;

  static AnalysisKey Key;

public:
  using Result = SeparateStorageAAResult;

  SeparateStorageAAResult run(Function &F, FunctionAnalysisManager &AM);
};

/// Legacy wrapper pass to provide the SeparateStorageAAResult object.
class SeparateStorageAAWrapperPass : public FunctionPass {
  std::unique_ptr<SeparateStorageAAResult> Result;

public:
  static char ID;

  SeparateStorageAAWrapperPass();

  SeparateStorageAAResult &getResult() { return *Result; }
  const SeparateStorageAAResult &getResult() const { return *Result; }

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

FunctionPass *createSeparateStorageAAWrapperPass();
} // namespace llvm

#endif // LLVM_ANALYSIS_SEPARATESTORAGEALIASANALYSIS
