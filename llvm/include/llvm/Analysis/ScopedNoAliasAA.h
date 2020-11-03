//===- ScopedNoAliasAA.h - Scoped No-Alias Alias Analysis -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This is the interface for a metadata-based scoped no-alias analysis.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_SCOPEDNOALIASAA_H
#define LLVM_ANALYSIS_SCOPEDNOALIASAA_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <memory>

namespace llvm {
class DominatorTree;

class Function;
class MDNode;
class MemoryLocation;

/// A simple AA result which uses scoped-noalias metadata to answer queries.
class ScopedNoAliasAAResult : public AAResultBase<ScopedNoAliasAAResult> {
  friend AAResultBase<ScopedNoAliasAAResult>;

public:
  ScopedNoAliasAAResult(DominatorTree *DT) : AAResultBase(), DT(DT) {}

  /// Handle invalidation events from the new pass manager.
  ///
  /// By definition, this result is stateless and so remains valid.
  bool invalidate(Function &F, const PreservedAnalyses &PA,
                  FunctionAnalysisManager::Invalidator &Inv);

  AliasResult alias(const MemoryLocation &LocA, const MemoryLocation &LocB,
                    AAQueryInfo &AAQI);
  ModRefInfo getModRefInfo(const CallBase *Call, const MemoryLocation &Loc,
                           AAQueryInfo &AAQI);
  ModRefInfo getModRefInfo(const CallBase *Call1, const CallBase *Call2,
                           AAQueryInfo &AAQI);

  // FIXME: This interface can be removed once the legacy-pass-manager support
  // is removed.
  void setDT(DominatorTree *DT) { this->DT = DT; }

private:
  bool mayAliasInScopes(const MDNode *Scopes, const MDNode *NoAlias) const;

  bool findCompatibleNoAlias(const Value *P, const MDNode *ANoAlias,
                             const MDNode *BNoAlias, const DataLayout &DL,
                             SmallPtrSetImpl<const Value *> &Visited,
                             SmallVectorImpl<Instruction *> &CompatibleSet,
                             int Depth = 0);
  bool noAliasByIntrinsic(const MDNode *ANoAlias, const Value *APtr,
                          const MDNode *BNoAlias, const Value *BPtr,
                          const CallBase *CallA, const CallBase *CallB,
                          AAQueryInfo &AAQI);

  DominatorTree *DT;
};

/// Analysis pass providing a never-invalidated alias analysis result.
class ScopedNoAliasAA : public AnalysisInfoMixin<ScopedNoAliasAA> {
  friend AnalysisInfoMixin<ScopedNoAliasAA>;

  static AnalysisKey Key;

public:
  using Result = ScopedNoAliasAAResult;

  ScopedNoAliasAAResult run(Function &F, FunctionAnalysisManager &AM);
};

/// Legacy wrapper pass to provide the ScopedNoAliasAAResult object.
class ScopedNoAliasAAWrapperPass : public ImmutablePass {
  std::unique_ptr<ScopedNoAliasAAResult> Result;

public:
  static char ID;

  ScopedNoAliasAAWrapperPass();

  ScopedNoAliasAAResult &getResult() {
    setDT();
    return *Result;
  }
  const ScopedNoAliasAAResult &getResult() const { return *Result; }

  bool doInitialization(Module &M) override;
  bool doFinalization(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  void setDT();
};

//===--------------------------------------------------------------------===//
//
// createScopedNoAliasAAWrapperPass - This pass implements metadata-based
// scoped noalias analysis.
//
ImmutablePass *createScopedNoAliasAAWrapperPass();

} // end namespace llvm

#endif // LLVM_ANALYSIS_SCOPEDNOALIASAA_H
