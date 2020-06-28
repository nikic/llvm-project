//===- SimplifyCFGPass.cpp - CFG Simplification Pass ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements dead code elimination and basic block merging, along
// with a collection of other peephole control flow optimizations.  For example:
//
//   * Removes basic blocks with no predecessors.
//   * Merges a basic block into its predecessor if there is only one and the
//     predecessor only has one successor.
//   * Eliminates PHI nodes for basic blocks with a single predecessor.
//   * Eliminates a basic block that only contains an unconditional branch.
//   * Changes invoke instructions to nounwind functions to be calls.
//   * Change things like "if (x) if (y)" into "if (x&y)".
//   * etc..
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Local.h"
#include <utility>
using namespace llvm;

#define DEBUG_TYPE "simplifycfg"

static cl::opt<unsigned> UserBonusInstThreshold(
    "bonus-inst-threshold", cl::Hidden, cl::init(1),
    cl::desc("Control the number of bonus instructions (default = 1)"));

static cl::opt<bool> UserKeepLoops(
    "keep-loops", cl::Hidden, cl::init(true),
    cl::desc("Preserve canonical loop structure (default = true)"));

static cl::opt<bool> UserSwitchToLookup(
    "switch-to-lookup", cl::Hidden, cl::init(false),
    cl::desc("Convert switches to lookup tables (default = false)"));

static cl::opt<bool> UserForwardSwitchCond(
    "forward-switch-cond", cl::Hidden, cl::init(false),
    cl::desc("Forward switch condition to phi ops (default = false)"));

static cl::opt<bool> UserSinkCommonInsts(
    "sink-common-insts", cl::Hidden, cl::init(false),
    cl::desc("Sink common instructions (default = false)"));

/// Maximum number of candidate blocks with same hash to consider for merging.
static const unsigned MaxBlockMergeCandidates = 32;

STATISTIC(NumSimpl, "Number of blocks simplified");
STATISTIC(NumBlocksMerged, "Number of blocks merged");

/// Check whether replacing BB with ReplacementBB would result in a CallBr
/// instruction with a duplicate destination in one of the predecessors.
// FIXME: See note in CodeGenPrepare.cpp.
bool wouldDuplicateCallBrDest(const BasicBlock &BB,
                              const BasicBlock &ReplacementBB) {
  for (const BasicBlock *Pred : predecessors(&BB)) {
    if (auto *CBI = dyn_cast<CallBrInst>(Pred->getTerminator()))
      for (const BasicBlock *Succ : successors(CBI))
        if (&ReplacementBB == Succ)
          return true;
  }
  return false;
}

/// If we have more than one empty (other than phi node) return blocks,
/// merge them together to promote recursive block merging.
static bool mergeEmptyReturnBlocks(Function &F) {
  bool Changed = false;

  BasicBlock *RetBlock = nullptr;

  // Scan all the blocks in the function, looking for empty return blocks.
  for (Function::iterator BBI = F.begin(), E = F.end(); BBI != E; ) {
    BasicBlock &BB = *BBI++;

    // Only look at return blocks.
    ReturnInst *Ret = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!Ret) continue;

    // Only look at the block if it is empty or the only other thing in it is a
    // single PHI node that is the operand to the return.
    if (Ret != &BB.front()) {
      // Check for something else in the block.
      BasicBlock::iterator I(Ret);
      --I;
      // Skip over debug info.
      while (isa<DbgInfoIntrinsic>(I) && I != BB.begin())
        --I;
      if (!isa<DbgInfoIntrinsic>(I) &&
          (!isa<PHINode>(I) || I != BB.begin() || Ret->getNumOperands() == 0 ||
           Ret->getOperand(0) != &*I))
        continue;
    }

    // If this is the first returning block, remember it and keep going.
    if (!RetBlock) {
      RetBlock = &BB;
      continue;
    }

    if (wouldDuplicateCallBrDest(BB, *RetBlock))
      continue;

    // Otherwise, we found a duplicate return block.  Merge the two.
    Changed = true;

    // Case when there is no input to the return or when the returned values
    // agree is trivial.  Note that they can't agree if there are phis in the
    // blocks.
    if (Ret->getNumOperands() == 0 ||
        Ret->getOperand(0) ==
          cast<ReturnInst>(RetBlock->getTerminator())->getOperand(0)) {
      BB.replaceAllUsesWith(RetBlock);
      BB.eraseFromParent();
      continue;
    }

    // If the canonical return block has no PHI node, create one now.
    PHINode *RetBlockPHI = dyn_cast<PHINode>(RetBlock->begin());
    if (!RetBlockPHI) {
      Value *InVal = cast<ReturnInst>(RetBlock->getTerminator())->getOperand(0);
      pred_iterator PB = pred_begin(RetBlock), PE = pred_end(RetBlock);
      RetBlockPHI = PHINode::Create(Ret->getOperand(0)->getType(),
                                    std::distance(PB, PE), "merge",
                                    &RetBlock->front());

      for (pred_iterator PI = PB; PI != PE; ++PI)
        RetBlockPHI->addIncoming(InVal, *PI);
      RetBlock->getTerminator()->setOperand(0, RetBlockPHI);
    }

    // Turn BB into a block that just unconditionally branches to the return
    // block.  This handles the case when the two return blocks have a common
    // predecessor but that return different things.
    RetBlockPHI->addIncoming(Ret->getOperand(0), &BB);
    BB.getTerminator()->eraseFromParent();
    BranchInst::Create(RetBlock, &BB);
  }

  return Changed;
}

class HashAccumulator64 {
  uint64_t Hash;

public:
  HashAccumulator64() { Hash = 0x6acaa36bef8325c5ULL; }
  void add(uint64_t V) { Hash = hashing::detail::hash_16_bytes(Hash, V); }
  uint64_t getHash() { return Hash; }
};

static uint64_t hashBlock(const BasicBlock &BB) {
  HashAccumulator64 Acc;
  for (const Instruction &I : BB)
    Acc.add(I.getOpcode());
  return Acc.getHash();
}

static bool canMergeBlocks(const BasicBlock &BB1, const BasicBlock &BB2) {
  // Quickly bail out if successors don't match.
  if (!std::equal(succ_begin(&BB1), succ_end(&BB1),
                  succ_begin(&BB2), succ_end(&BB2)))
    return false;

  // Map from instructions in one block to instructions in the other.
  SmallDenseMap<const Instruction *, const Instruction *> Map;
  auto ValuesEqual = [&Map](const Value *V1, const Value *V2) {
    if (V1 == V2)
      return true;;

    if (const auto *I1 = dyn_cast<Instruction>(V1))
      if (const auto *I2 = dyn_cast<Instruction>(V2))
        if (const Instruction *Mapped = Map.lookup(I1))
          if (Mapped == I2)
            return true;

    return false;
  };

  auto InstructionsEqual = [&](const Instruction &I1, const Instruction &I2) {
    if (!I1.isSameOperationAs(&I2))
      return false;

    if (const auto *Call = dyn_cast<CallBase>(&I1))
      if (Call->cannotMerge())
        return false;

    if (!std::equal(I1.op_begin(), I1.op_end(), I2.op_begin(), I2.op_end(),
                    ValuesEqual))
      return false;

    if (const PHINode *PHI1 = dyn_cast<PHINode>(&I2)) {
      const PHINode *PHI2 = cast<PHINode>(&I2);
      return std::equal(PHI1->block_begin(), PHI1->block_end(),
                        PHI2->block_begin(), PHI2->block_end());
    }

    if (!I1.use_empty())
      Map.insert({&I1, &I2});
    return true;
  };
  auto It1 = BB1.instructionsWithoutDebug();
  auto It2 = BB2.instructionsWithoutDebug();
  if (!std::equal(It1.begin(), It1.end(), It2.begin(), It2.end(),
                  InstructionsEqual))
    return false;

  // Make sure phi values in successor blocks match.
  for (const BasicBlock *Succ : successors(&BB1)) {
    for (const PHINode &Phi : Succ->phis()) {
      const Value *Incoming1 = Phi.getIncomingValueForBlock(&BB1);
      const Value *Incoming2 = Phi.getIncomingValueForBlock(&BB2);
      if (!ValuesEqual(Incoming1, Incoming2))
        return false;
    }
  }

  if (wouldDuplicateCallBrDest(BB1, BB2))
    return false;

  return true;
}

static bool tryMergeTwoBlocks(BasicBlock &BB1, BasicBlock &BB2) {
  if (!canMergeBlocks(BB1, BB2))
    return false;

  // We will keep BB1 and drop BB2. Merge metadata and attributes.
  for (auto Insts : llvm::zip(BB1, BB2)) {
    Instruction &I1 = std::get<0>(Insts);
    Instruction &I2 = std::get<1>(Insts);
    I1.andIRFlags(&I2);
    combineMetadataForCSE(&I1, &I2, true);
    I1.applyMergedLocation(I1.getDebugLoc(), I2.getDebugLoc());
  }

  // Store predecessors, because they will be modified in this loop.
  SmallVector<BasicBlock *, 4> Preds(predecessors(&BB2));
  for (BasicBlock *Pred : Preds)
    Pred->getTerminator()->replaceSuccessorWith(&BB2, &BB1);

  for (BasicBlock *Succ : successors(&BB2))
    Succ->removePredecessor(&BB2);

  BB2.eraseFromParent();
  return true;
}

static bool mergeIdenticalBlocks(Function &F) {
  bool Changed = false;
  SmallDenseMap<uint64_t, SmallVector<BasicBlock *, 2>> SameHashBlocks;

  // Process blocks in post-order, because merging successors may enable to
  // merge predecessors.
  SmallVector<BasicBlock *, 8> PostOrder(post_order(&F));
  for (BasicBlock *BB : PostOrder) {
    // The entry block cannot be merged.
    if (BB == &F.getEntryBlock())
      continue;

    // Identify potential merging candidates based on a basic block hash.
    bool Merged = false;
    auto &Blocks = SameHashBlocks.try_emplace(hashBlock(*BB)).first->second;
    for (BasicBlock *Block : Blocks) {
      if (tryMergeTwoBlocks(*Block, *BB)) {
        Merged = true;
        ++NumBlocksMerged;
        break;
      }
    }

    Changed |= Merged;
    if (!Merged && Blocks.size() < MaxBlockMergeCandidates)
      Blocks.push_back(BB);
  }

  return Changed;
}

/// Call SimplifyCFG on all the blocks in the function,
/// iterating until no more changes are made.
static bool iterativelySimplifyCFG(Function &F, const TargetTransformInfo &TTI,
                                   const SimplifyCFGOptions &Options) {
  bool Changed = false;
  bool LocalChange = true;

  SmallVector<std::pair<const BasicBlock *, const BasicBlock *>, 32> Edges;
  FindFunctionBackedges(F, Edges);
  SmallPtrSet<BasicBlock *, 16> LoopHeaders;
  for (unsigned i = 0, e = Edges.size(); i != e; ++i)
    LoopHeaders.insert(const_cast<BasicBlock *>(Edges[i].second));

  while (LocalChange) {
    LocalChange = false;

    // Loop over all of the basic blocks and remove them if they are unneeded.
    for (Function::iterator BBIt = F.begin(); BBIt != F.end(); ) {
      if (simplifyCFG(&*BBIt++, TTI, Options, &LoopHeaders)) {
        LocalChange = true;
        ++NumSimpl;
      }
    }
    Changed |= LocalChange;
  }
  return Changed;
}

static bool simplifyFunctionCFG(Function &F, const TargetTransformInfo &TTI,
                                const SimplifyCFGOptions &Options) {
  bool EverChanged = removeUnreachableBlocks(F);
  EverChanged |= mergeEmptyReturnBlocks(F);
  EverChanged |= mergeIdenticalBlocks(F);
  EverChanged |= iterativelySimplifyCFG(F, TTI, Options);

  // If neither pass changed anything, we're done.
  if (!EverChanged) return false;

  // iterativelySimplifyCFG can (rarely) make some loops dead.  If this happens,
  // removeUnreachableBlocks is needed to nuke them, which means we should
  // iterate between the two optimizations.  We structure the code like this to
  // avoid rerunning iterativelySimplifyCFG if the second pass of
  // removeUnreachableBlocks doesn't do anything.
  if (!removeUnreachableBlocks(F))
    return true;

  do {
    EverChanged = iterativelySimplifyCFG(F, TTI, Options);
    EverChanged |= removeUnreachableBlocks(F);
  } while (EverChanged);

  return true;
}

// Command-line settings override compile-time settings.
SimplifyCFGPass::SimplifyCFGPass(const SimplifyCFGOptions &Opts) {
  Options.BonusInstThreshold = UserBonusInstThreshold.getNumOccurrences()
                                   ? UserBonusInstThreshold
                                   : Opts.BonusInstThreshold;
  Options.ForwardSwitchCondToPhi = UserForwardSwitchCond.getNumOccurrences()
                                       ? UserForwardSwitchCond
                                       : Opts.ForwardSwitchCondToPhi;
  Options.ConvertSwitchToLookupTable = UserSwitchToLookup.getNumOccurrences()
                                           ? UserSwitchToLookup
                                           : Opts.ConvertSwitchToLookupTable;
  Options.NeedCanonicalLoop = UserKeepLoops.getNumOccurrences()
                                  ? UserKeepLoops
                                  : Opts.NeedCanonicalLoop;
  Options.SinkCommonInsts = UserSinkCommonInsts.getNumOccurrences()
                                ? UserSinkCommonInsts
                                : Opts.SinkCommonInsts;
}

PreservedAnalyses SimplifyCFGPass::run(Function &F,
                                       FunctionAnalysisManager &AM) {
  auto &TTI = AM.getResult<TargetIRAnalysis>(F);
  Options.AC = &AM.getResult<AssumptionAnalysis>(F);
  if (!simplifyFunctionCFG(F, TTI, Options))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<GlobalsAA>();
  return PA;
}

namespace {
struct CFGSimplifyPass : public FunctionPass {
  static char ID;
  SimplifyCFGOptions Options;
  std::function<bool(const Function &)> PredicateFtor;

  CFGSimplifyPass(unsigned Threshold = 1, bool ForwardSwitchCond = false,
                  bool ConvertSwitch = false, bool KeepLoops = true,
                  bool SinkCommon = false,
                  std::function<bool(const Function &)> Ftor = nullptr)
      : FunctionPass(ID), PredicateFtor(std::move(Ftor)) {

    initializeCFGSimplifyPassPass(*PassRegistry::getPassRegistry());

    // Check for command-line overrides of options for debug/customization.
    Options.BonusInstThreshold = UserBonusInstThreshold.getNumOccurrences()
                                    ? UserBonusInstThreshold
                                    : Threshold;

    Options.ForwardSwitchCondToPhi = UserForwardSwitchCond.getNumOccurrences()
                                         ? UserForwardSwitchCond
                                         : ForwardSwitchCond;

    Options.ConvertSwitchToLookupTable = UserSwitchToLookup.getNumOccurrences()
                                             ? UserSwitchToLookup
                                             : ConvertSwitch;

    Options.NeedCanonicalLoop =
        UserKeepLoops.getNumOccurrences() ? UserKeepLoops : KeepLoops;

    Options.SinkCommonInsts = UserSinkCommonInsts.getNumOccurrences()
                                  ? UserSinkCommonInsts
                                  : SinkCommon;
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F) || (PredicateFtor && !PredicateFtor(F)))
      return false;

    Options.AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
    if (F.hasFnAttribute(Attribute::OptForFuzzing)) {
      Options.setSimplifyCondBranch(false)
             .setFoldTwoEntryPHINode(false);
    } else {
      Options.setSimplifyCondBranch(true)
             .setFoldTwoEntryPHINode(true);
    }

    auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
    return simplifyFunctionCFG(F, TTI, Options);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
  }
};
}

char CFGSimplifyPass::ID = 0;
INITIALIZE_PASS_BEGIN(CFGSimplifyPass, "simplifycfg", "Simplify the CFG", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_END(CFGSimplifyPass, "simplifycfg", "Simplify the CFG", false,
                    false)

// Public interface to the CFGSimplification pass
FunctionPass *
llvm::createCFGSimplificationPass(unsigned Threshold, bool ForwardSwitchCond,
                                  bool ConvertSwitch, bool KeepLoops,
                                  bool SinkCommon,
                                  std::function<bool(const Function &)> Ftor) {
  return new CFGSimplifyPass(Threshold, ForwardSwitchCond, ConvertSwitch,
                             KeepLoops, SinkCommon, std::move(Ftor));
}
