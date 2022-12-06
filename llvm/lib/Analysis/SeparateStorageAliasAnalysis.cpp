//===- SeparateStorageAliasAnalysis.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the separate storage alias analysis pass.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/SeparateStorageAliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<bool> EnableSeparateStorageAA("enable-separate-storage-aa",
                                             cl::Hidden, cl::init(true));

AnalysisKey SeparateStorageAA::Key;

SeparateStorageAAResult::SeparateStorageAAResult(AssumptionCache &AC,
                                                 DominatorTree &DT)
    : AC(AC), DT(DT) {}

bool SeparateStorageAAResult::invalidate(
    Function &Fn, const PreservedAnalyses &PA,
    FunctionAnalysisManager::Invalidator &Inv) {
  return Inv.invalidate<AssumptionAnalysis>(Fn, PA) ||
         Inv.invalidate<DominatorTreeAnalysis>(Fn, PA);
}

AliasResult SeparateStorageAAResult::alias(const MemoryLocation &LocA,
                                           const MemoryLocation &LocB,
                                           AAQueryInfo &AAQI,
                                           const Instruction *I) {
  if (!EnableSeparateStorageAA)
    return AliasResult::MayAlias;

  if (!I)
    return AliasResult::MayAlias;

  const Value *PtrA = LocA.Ptr;
  const Value *PtrB = LocB.Ptr;
  if (!PtrA || !PtrB)
    return AliasResult::MayAlias;

  const Value *UnderlyingA = getUnderlyingObject(PtrA);
  const Value *UnderlyingB = getUnderlyingObject(PtrB);

  for (auto &AssumeVH : AC.assumptions()) {
    if (!AssumeVH)
      continue;

    AssumeInst *Assume = cast<AssumeInst>(AssumeVH);

    for (unsigned Idx = 0; Idx < Assume->getNumOperandBundles(); Idx++) {
      OperandBundleUse OBU = Assume->getOperandBundleAt(Idx);
      if (OBU.getTagName() == "separate_storage") {
        assert(OBU.Inputs.size() == 2);
        const Value *HintA = OBU.Inputs[0].get();
        const Value *HintB = OBU.Inputs[1].get();
        const Value *UnderlyingHintA = getUnderlyingObject(HintA);
        const Value *UnderlyingHintB = getUnderlyingObject(HintB);

        if (((UnderlyingA == UnderlyingHintA &&
              UnderlyingB == UnderlyingHintB) ||
             (UnderlyingA == UnderlyingHintB &&
              UnderlyingB == UnderlyingHintA)) &&
            isValidAssumeForContext(Assume, I, &DT))
          return AliasResult::NoAlias;
        break;
      }
    }
  }
  return AliasResult::MayAlias;
}

SeparateStorageAAResult SeparateStorageAA::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  return SeparateStorageAAResult(AC, DT);
}

char SeparateStorageAAWrapperPass::ID = 0;
INITIALIZE_PASS(SeparateStorageAAWrapperPass, "separatestorage-aa",
                "Separate Storage Alias Analysis", false, true)

FunctionPass *llvm::createSeparateStorageAAWrapperPass() {
  return new SeparateStorageAAWrapperPass();
}

SeparateStorageAAWrapperPass::SeparateStorageAAWrapperPass()
    : FunctionPass(ID) {
  initializeSeparateStorageAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

bool SeparateStorageAAWrapperPass::runOnFunction(Function &F) {
  auto &ACT = getAnalysis<AssumptionCacheTracker>();
  auto &DTWP = getAnalysis<DominatorTreeWrapperPass>();

  Result.reset(new SeparateStorageAAResult(ACT.getAssumptionCache(F),
                                           DTWP.getDomTree()));
  return false;
}

void SeparateStorageAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequiredTransitive<AssumptionCacheTracker>();
  AU.addRequiredTransitive<DominatorTreeWrapperPass>();
}
