//===- BreakCriticalEdges.cpp - Critical Edge Elimination Pass ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// BreakCriticalEdges pass - Break all of the critical edges in the CFG by
// inserting a dummy basic block.  This pass may be "required" by passes that
// cannot deal with critical edges.  For this usage, the structure type is
// forward declared.  This pass obviously invalidates the CFG, but can update
// dominator trees.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/BreakCriticalEdges.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

#define DEBUG_TYPE "break-crit-edges"

STATISTIC(NumBroken, "Number of blocks inserted");

namespace {
struct BreakCriticalEdges : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  BreakCriticalEdges() : FunctionPass(ID) {
    initializeBreakCriticalEdgesPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    auto *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>();
    auto *DT = DTWP ? &DTWP->getDomTree() : nullptr;

    auto *PDTWP = getAnalysisIfAvailable<PostDominatorTreeWrapperPass>();
    auto *PDT = PDTWP ? &PDTWP->getPostDomTree() : nullptr;

    auto *LIWP = getAnalysisIfAvailable<LoopInfoWrapperPass>();
    auto *LI = LIWP ? &LIWP->getLoopInfo() : nullptr;
    unsigned N = SplitAllCriticalEdges(
        F, CriticalEdgeSplittingOptions(DT, LI, nullptr, PDT));
    NumBroken += N;
    return N > 0;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();

    // No loop canonicalization guarantees are broken by this pass.
    AU.addPreservedID(LoopSimplifyID);
  }
};
} // namespace

char BreakCriticalEdges::ID = 0;
INITIALIZE_PASS(BreakCriticalEdges, "break-crit-edges",
                "Break critical edges in CFG", false, false)

// Publicly exposed interface to pass...
char &llvm::BreakCriticalEdgesID = BreakCriticalEdges::ID;

FunctionPass *llvm::createBreakCriticalEdgesPass() {
  return new BreakCriticalEdges();
}

PreservedAnalyses BreakCriticalEdgesPass::run(Function &F,
                                              FunctionAnalysisManager &AM) {
  auto *DT = AM.getCachedResult<DominatorTreeAnalysis>(F);
  auto *LI = AM.getCachedResult<LoopAnalysis>(F);
  unsigned N = SplitAllCriticalEdges(F, CriticalEdgeSplittingOptions(DT, LI));
  NumBroken += N;
  if (N == 0)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<DominatorTreeAnalysis>();
  PA.preserve<LoopAnalysis>();
  return PA;
}

//===----------------------------------------------------------------------===//
//    Implementation of the external critical edge manipulation functions
//===----------------------------------------------------------------------===//

BasicBlock *llvm::SplitCriticalEdge(Instruction *TI, unsigned SuccNum,
                                    const CriticalEdgeSplittingOptions &Options,
                                    const Twine &BBName) {
  if (!isCriticalEdge(TI, SuccNum, Options.MergeIdenticalEdges))
    return nullptr;

  return SplitKnownCriticalEdge(TI, SuccNum, Options, BBName);
}

BasicBlock *
llvm::SplitKnownCriticalEdge(Instruction *TI, unsigned SuccNum,
                             const CriticalEdgeSplittingOptions &Options,
                             const Twine &BBName) {
  BasicBlock *TIBB = TI->getParent();
  BasicBlock *DestBB = TI->getSuccessor(SuccNum);

  // Splitting the critical edge to a pad block is non-trivial.
  // Don't do it in this generic function.
  if (DestBB->isEHPad())
    return nullptr;

  if (Options.IgnoreUnreachableDests &&
      isa<UnreachableInst>(DestBB->getFirstNonPHIOrLifetime()))
    return nullptr;

  auto *LI = Options.LI;
  SmallVector<BasicBlock *, 4> LoopPreds;
  // Check if extra modifications will be required to preserve loop-simplify
  // form after splitting.
  if (LI) {
    if (Loop *TIL = LI->getLoopFor(TIBB)) {

      // The only way that we can break LoopSimplify form by splitting a
      // critical edge is if after the split there exists some edge from TIL to
      // DestBB *and* the only edge into DestBB from outside of TIL is that of
      // NewBB. If the first isn't true, then LoopSimplify still holds, NewBB
      // is the new exit block and it has no non-loop predecessors. If the
      // second isn't true, then DestBB was not in LoopSimplify form prior to
      // the split as it had a non-loop predecessor. In both of these cases,
      // the predecessor must be directly in TIL, not in a subloop, or again
      // LoopSimplify doesn't hold.
      for (BasicBlock *P : predecessors(DestBB)) {
        if (P == TIBB)
          continue; // The new block is known.
        if (LI->getLoopFor(P) != TIL) {
          // No need to re-simplify, it wasn't to start with.
          LoopPreds.clear();
          break;
        }
        LoopPreds.push_back(P);
      }
    }
  }

  // Create a new basic block, linking it into the CFG.
  BasicBlock *NewBB = nullptr;
  if (BBName.str() != "")
    NewBB = BasicBlock::Create(TI->getContext(), BBName);
  else
    NewBB = BasicBlock::Create(TI->getContext(), TIBB->getName() + "." +
                                                     DestBB->getName() +
                                                     "_crit_edge");
  // Create our unconditional branch.
  UncondBrInst *NewBI = UncondBrInst::Create(DestBB, NewBB);
  NewBI->setDebugLoc(TI->getDebugLoc());
  if (auto *LoopMD = TI->getMetadata(LLVMContext::MD_loop))
    NewBI->setMetadata(LLVMContext::MD_loop, LoopMD);

  // Insert the block into the function... right after the block TI lives in.
  Function &F = *TIBB->getParent();
  Function::iterator FBBI = TIBB->getIterator();
  F.insert(++FBBI, NewBB);

  // Branch to the new block, breaking the edge.
  TI->setSuccessor(SuccNum, NewBB);

  // If there are any PHI nodes in DestBB, we need to update them so that they
  // merge incoming values from NewBB instead of from TIBB.
  {
    unsigned BBIdx = 0;
    for (BasicBlock::iterator I = DestBB->begin(); isa<PHINode>(I); ++I) {
      // We no longer enter through TIBB, now we come in through NewBB.
      // Revector exactly one entry in the PHI node that used to come from
      // TIBB to come from NewBB.
      PHINode *PN = cast<PHINode>(I);

      // Reuse the previous value of BBIdx if it lines up.  In cases where we
      // have multiple phi nodes with *lots* of predecessors, this is a speed
      // win because we don't have to scan the PHI looking for TIBB.  This
      // happens because the BB list of PHI nodes are usually in the same
      // order.
      if (PN->getIncomingBlock(BBIdx) != TIBB)
        BBIdx = PN->getBasicBlockIndex(TIBB);
      PN->setIncomingBlock(BBIdx, NewBB);
    }
  }

  unsigned NumSplitIdenticalEdges = 1;

  // If there are any other edges from TIBB to DestBB, update those to go
  // through the split block, making those edges non-critical as well (and
  // reducing the number of phi entries in the DestBB if relevant).
  if (Options.MergeIdenticalEdges) {
    for (unsigned i = SuccNum+1, e = TI->getNumSuccessors(); i != e; ++i) {
      if (TI->getSuccessor(i) != DestBB) continue;

      // Remove an entry for TIBB from DestBB phi nodes.
      DestBB->removePredecessor(TIBB, Options.KeepOneInputPHIs);

      // We found another edge to DestBB, go to NewBB instead.
      TI->setSuccessor(i, NewBB);

      // Record the number of split identical edges to DestBB.
      NumSplitIdenticalEdges++;
    }
  }

  // If we have nothing to update, just return.
  auto *DT = Options.DT;
  auto *PDT = Options.PDT;
  auto *MSSAU = Options.MSSAU;
  if (MSSAU)
    MSSAU->wireOldPredecessorsToNewImmediatePredecessor(
        DestBB, NewBB, {TIBB}, Options.MergeIdenticalEdges);

  if (!DT && !PDT && !LI)
    return NewBB;

  if (DT || PDT) {
    // Update the DominatorTree.
    //       ---> NewBB -----\
    //      /                 V
    //  TIBB -------\\------> DestBB
    //
    // First, inform the DT about the new path from TIBB to DestBB via NewBB,
    // then delete the old edge from TIBB to DestBB. By doing this in that order
    // DestBB stays reachable in the DT the whole time and its subtree doesn't
    // get disconnected.
    SmallVector<DominatorTree::UpdateType, 3> Updates;
    Updates.push_back({DominatorTree::Insert, TIBB, NewBB});
    Updates.push_back({DominatorTree::Insert, NewBB, DestBB});
    if (!llvm::is_contained(successors(TIBB), DestBB))
      Updates.push_back({DominatorTree::Delete, TIBB, DestBB});

    if (DT)
      DT->applyUpdates(Updates);
    if (PDT)
      PDT->applyUpdates(Updates);
  }

  // Update LoopInfo if it is around.
  if (LI) {
    if (Loop *TIL = LI->getLoopFor(TIBB)) {
      // If one or the other blocks were not in a loop, the new block is not
      // either, and thus LI doesn't need to be updated.
      if (Loop *DestLoop = LI->getLoopFor(DestBB)) {
        if (TIL == DestLoop) {
          // Both in the same loop, the NewBB joins loop.
          DestLoop->addBasicBlockToLoop(NewBB, *LI);
        } else if (TIL->contains(DestLoop)) {
          // Edge from an outer loop to an inner loop.  Add to the outer loop.
          TIL->addBasicBlockToLoop(NewBB, *LI);
        } else if (DestLoop->contains(TIL)) {
          // Edge from an inner loop to an outer loop.  Add to the outer loop.
          DestLoop->addBasicBlockToLoop(NewBB, *LI);
        } else {
          // Edge from two loops with no containment relation.  Because these
          // are natural loops, we know that the destination block must be the
          // header of its loop (adding a branch into a loop elsewhere would
          // create an irreducible loop).
          assert(DestLoop->getHeader() == DestBB &&
                 "Should not create irreducible loops!");
          if (Loop *P = DestLoop->getParentLoop())
            P->addBasicBlockToLoop(NewBB, *LI);
        }
      }

      // If TIBB is in a loop and DestBB is outside of that loop, we may need
      // to update LoopSimplify form and LCSSA form.
      if (!TIL->contains(DestBB)) {
        assert(!TIL->contains(NewBB) &&
               "Split point for loop exit is contained in loop!");

        // Update LCSSA form in the newly created exit block.
        if (Options.PreserveLCSSA) {
          // If > 1 identical edges to be split, we need to introduce the same
          // number of the incoming blocks for the new PHINode.
          createPHIsForSplitLoopExit(
              SmallVector<BasicBlock *, 4>(NumSplitIdenticalEdges, TIBB), NewBB,
              DestBB);
        }

        if (!LoopPreds.empty()) {
          assert(!DestBB->isEHPad() && "We don't split edges to EH pads!");
          BasicBlock *NewExitBB = SplitBlockPredecessors(
              DestBB, LoopPreds, "split", DT, LI, MSSAU, Options.PreserveLCSSA);
          if (Options.PreserveLCSSA)
            createPHIsForSplitLoopExit(LoopPreds, NewExitBB, DestBB);
        }
      }
    }
  }

  return NewBB;
}
