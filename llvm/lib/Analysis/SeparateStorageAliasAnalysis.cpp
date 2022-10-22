// facebook begin T130678741
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
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

static cl::opt<bool> EnableSeparateStorageAA("enable-separate-storage-aa",
                                             cl::Hidden, cl::init(true));

SeparateStorageAAResult::SeparateStorageAAResult(Function &F) : F(F) {}

SeparateStorageAAResult::State &SeparateStorageAAResult::getState() {
  if (!CachedState) {
    CachedState.emplace();
    CachedState.value().DT.recalculate(F);
    for (BasicBlock &B : F)
      for (Instruction &I : B)
        if (SeparateStorageInst *SSI = dyn_cast<SeparateStorageInst>(&I))
          CachedState.value().Hints.push_back(SSI);
  }
  return *CachedState;
}

AliasResult SeparateStorageAAResult::aliasAt(const MemoryLocation &LocA,
                                             const MemoryLocation &LocB,
                                             const Instruction *I,
                                             AAQueryInfo &AAQI) {
  if (!EnableSeparateStorageAA) {
    return AAResultBase::aliasAt(LocA, LocB, I, AAQI);
  }

  const Value *PtrA = LocA.Ptr;
  const Value *PtrB = LocB.Ptr;
  if (!PtrA || !PtrB)
    return AAResultBase::aliasAt(LocA, LocB, I, AAQI);

  auto &State = getState();
  const Value *UnderlyingA = getUnderlyingObject(PtrA);
  const Value *UnderlyingB = getUnderlyingObject(PtrB);

  for (WeakVH &VH : State.Hints) {
    if (!VH)
      continue;
    auto *SSI = cast<SeparateStorageInst>(VH);
    const Value *Hint0 = SSI->getOperand(0);
    const Value *Hint1 = SSI->getOperand(1);
    const Value *UnderlyingHint0 = getUnderlyingObject(Hint0);
    const Value *UnderlyingHint1 = getUnderlyingObject(Hint1);

    if (((UnderlyingA == UnderlyingHint0 && UnderlyingB == UnderlyingHint1) ||
         (UnderlyingA == UnderlyingHint1 && UnderlyingB == UnderlyingHint0)) &&
        isValidAssumeForContext(SSI, I, &State.DT))
      return AliasResult::NoAlias;
  }
  return AAResultBase::aliasAt(LocA, LocB, I, AAQI);
}

AnalysisKey SeparateStorageAA::Key;

SeparateStorageAAResult SeparateStorageAA::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  return SeparateStorageAAResult(F);
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
  Result.reset(new SeparateStorageAAResult(F));
  return false;
}

void SeparateStorageAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}
// facebook end T130678741
