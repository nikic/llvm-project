// facebook begin T130678741
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
#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Pass.h"

namespace llvm {
/// A simple AA result that uses calls to the separate storage intrinsics.
class SeparateStorageAAResult : public AAResultBase {
public:
  SeparateStorageAAResult(Function &F);

  AliasResult aliasAt(const MemoryLocation &LocA, const MemoryLocation &LocB,
                      const Instruction *I, AAQueryInfo &AAQI);

  bool invalidate(Function &, const PreservedAnalyses &,
                  FunctionAnalysisManager::Invalidator &) {
    // The AA framework is otherwise stateless, and AA-dependent passes have
    // gotten a little bit sloppy in their consideration of whether or not
    // they're effective without alias analysis (and so rely on getCachedResult
    // instead of getResult sort of haphazardly). This is extra bad in our case,
    // since if we get invalidate it also invalidates the AAResults we're
    // associated with, so we drop the stateless analyses too.
    //
    // For now, we just recompute on demand. We do this lazily, in the hope
    // that maybe we can avoid it entirely.
    //
    // There's some ways we can make this less wasteful:
    // - Generalize the AssumptionCache machinery work for separate-storage
    //   assumptions too.
    // - Make the PassManager know about "partially invalidated" results, which
    //   then get recomputed the next time an analysis is requested via
    //   getResult rather than getCachedResult.
    // - Do a (one-time) module scan for separate-storage assumptions. If none
    //   are present, we can skip any function-level scanning.
    CachedState.reset();
    return false;
  }

private:
  struct State {
    DominatorTree DT;
    SmallVector<WeakVH, 4> Hints;
  };

  Function &F;
  Optional<State> CachedState;

  State &getState();
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
// facebook end T130678741
