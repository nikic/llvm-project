//===- IndirectBrExpandPass.cpp - Expand indirectswitch to switch ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// Implements an expansion pass to turn `indirectswitch` instructions in the
/// IR into `switch` instructions in the same block, over integers that replace
/// the `edgeaddress` constants used as its case values.
///
/// While this is generically useful if a target is unable to codegen indirect
/// branches natively, it is primarily useful when there is some desire to get
/// the builtin non-jump-table lowering of a switch even when the input source
/// contained an explicit indirect branch construct.
///
/// Note that it doesn't make any sense to enable this pass unless a target also
/// disables jump-table lowering of switches. Doing that is likely to pessimize
/// the code.
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/CodeGen/IndirectBrExpand.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/ProfDataUtils.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "indirectbr-expand"

namespace {

class IndirectBrExpandLegacyPass : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid

  IndirectBrExpandLegacyPass() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<DominatorTreeWrapperPass>();
  }

  bool runOnFunction(Function &F) override;
};

} // end anonymous namespace

static bool runImpl(Function &F, DomTreeUpdater *DTU);

PreservedAnalyses IndirectBrExpandPass::run(Function &F,
                                            FunctionAnalysisManager &FAM) {
  auto *STI = TM->getSubtargetImpl(F);
  if (!STI->enableIndirectBrExpand())
    return PreservedAnalyses::all();

  auto *DT = FAM.getCachedResult<DominatorTreeAnalysis>(F);
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Lazy);

  bool Changed = runImpl(F, DT ? &DTU : nullptr);
  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

char IndirectBrExpandLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(IndirectBrExpandLegacyPass, DEBUG_TYPE,
                      "Expand indirectswitch instructions", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(IndirectBrExpandLegacyPass, DEBUG_TYPE,
                    "Expand indirectswitch instructions", false, false)

FunctionPass *llvm::createIndirectBrExpandPass() {
  return new IndirectBrExpandLegacyPass();
}

/// Whether \p U is the use of an edgeaddress as an indirectswitch case value.
static bool isIndirectSwitchCaseUse(const Use &U) {
  return isa<IndirectSwitchInst>(U.getUser()) && U.getOperandNo() != 0;
}

/// Replace \p ISI with a switch over small integers, and replace the
/// edgeaddress constants used as its case values with those integers.
static void
expandIndirectSwitch(IndirectSwitchInst *ISI,
                     SmallVectorImpl<DominatorTree::UpdateType> &Updates) {
  BasicBlock *BB = ISI->getParent();
  Function &F = *BB->getParent();
  const DataLayout &DL = F.getDataLayout();

  // Only cases whose edgeaddress is used for something other than the case
  // value itself can be taken.
  SmallVector<unsigned> LiveCases;
  for (unsigned I = 0, E = ISI->getNumCases(); I != E; ++I) {
    EdgeAddress *EA = ISI->getCaseValue(I);
    EA->removeDeadConstantUsers();
    if (!all_of(EA->uses(), isIndirectSwitchCaseUse))
      LiveCases.push_back(I);
    else
      ISI->getSuccessor(I)->removePredecessor(BB, /*KeepOneInputPHIs=*/true);
  }

  SmallSetVector<BasicBlock *, 8> OldSuccs(llvm::from_range, successors(ISI));
  SmallPtrSet<BasicBlock *, 8> NewSuccs;
  SmallVector<std::pair<EdgeAddress *, Constant *>> Replacements;
  if (LiveCases.empty()) {
    (void)new UnreachableInst(F.getContext(), ISI->getIterator());
  } else {
    auto *ITy =
        cast<IntegerType>(DL.getIntPtrType(ISI->getAddress()->getType()));
    Value *SwitchValue = CastInst::CreatePointerCast(
        ISI->getAddress(), ITy,
        Twine(ISI->getAddress()->getName()) + ".switch_cast",
        ISI->getIterator());
    // Use the first live case as the default destination; values not matching
    // any case are undefined behavior. Integers start at 1, as null may be
    // compared with an edgeaddress.
    BasicBlock *DefaultBB = ISI->getSuccessor(LiveCases[0]);
    auto *SI = SwitchInst::Create(SwitchValue, DefaultBB, LiveCases.size() - 1,
                                  ISI->getIterator());
    NewSuccs.insert(DefaultBB);
    for (auto [Idx, CaseIdx] : enumerate(LiveCases)) {
      ConstantInt *C = ConstantInt::get(ITy, Idx + 1);
      if (Idx != 0) {
        SI->addCase(C, ISI->getSuccessor(CaseIdx));
        NewSuccs.insert(ISI->getSuccessor(CaseIdx));
      }
      EdgeAddress *EA = ISI->getCaseValue(CaseIdx);
      Replacements.push_back({EA, ConstantExpr::getIntToPtr(C, EA->getType())});
    }

    SmallVector<uint32_t> Weights;
    if (extractBranchWeights(*ISI, Weights)) {
      SmallVector<uint32_t> NewWeights;
      for (unsigned CaseIdx : LiveCases)
        NewWeights.push_back(Weights[CaseIdx]);
      setBranchWeights(*SI, NewWeights, /*IsExpected=*/false);
    } else {
      setExplicitlyUnknownBranchWeightsIfProfiled(*SI, DEBUG_TYPE);
    }
  }

  for (BasicBlock *Succ : OldSuccs)
    if (!NewSuccs.contains(Succ))
      Updates.push_back({DominatorTree::Delete, BB, Succ});
  ISI->eraseFromParent();

  for (auto [EA, C] : Replacements)
    EA->replaceAllUsesWith(C);
}

static bool runImpl(Function &F, DomTreeUpdater *DTU) {
  SmallVector<IndirectSwitchInst *, 1> IndirectSwitches;
  for (BasicBlock &BB : F)
    if (auto *ISI = dyn_cast<IndirectSwitchInst>(BB.getTerminator()))
      IndirectSwitches.push_back(ISI);
  SmallVector<DominatorTree::UpdateType, 8> Updates;
  for (IndirectSwitchInst *ISI : IndirectSwitches)
    expandIndirectSwitch(ISI, Updates);
  if (DTU)
    DTU->applyUpdates(Updates);
  return !IndirectSwitches.empty();
}

bool IndirectBrExpandLegacyPass::runOnFunction(Function &F) {
  auto *TPC = getAnalysisIfAvailable<TargetPassConfig>();
  if (!TPC)
    return false;

  auto &TM = TPC->getTM<TargetMachine>();
  auto &STI = *TM.getSubtargetImpl(F);
  if (!STI.enableIndirectBrExpand())
    return false;

  std::optional<DomTreeUpdater> DTU;
  if (auto *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>())
    DTU.emplace(DTWP->getDomTree(), DomTreeUpdater::UpdateStrategy::Lazy);

  return runImpl(F, DTU ? &*DTU : nullptr);
}
