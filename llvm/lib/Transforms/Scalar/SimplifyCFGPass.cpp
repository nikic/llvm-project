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

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/SimplifyCFGOptions.h"
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

static cl::opt<bool> UserHoistCommonInsts(
    "hoist-common-insts", cl::Hidden, cl::init(false),
    cl::desc("hoist common instructions (default = false)"));

static cl::opt<bool> UserSinkCommonInsts(
    "sink-common-insts", cl::Hidden, cl::init(false),
    cl::desc("Sink common instructions (default = false)"));


STATISTIC(NumSimpl, "Number of blocks simplified");

namespace {

struct CompatibleSets {
  using SetTy = SmallVector<CallInst *, 2>;

  SmallVector<SetTy, 1> Sets;

  static bool shouldBelongToSameSet(ArrayRef<CallInst *> Calls);

  SetTy &getCompatibleSet(CallInst *II);

  void insert(CallInst *II);
};

CompatibleSets::SetTy &CompatibleSets::getCompatibleSet(CallInst *II) {
  // Perform a linear scan over all the existing sets, see if the new `call`
  // is compatible with any particular set. Since we know that all the `calls`
  // within a set are compatible, only check the first `call` in each set.
  // WARNING: at worst, this has quadratic complexity.
  for (CompatibleSets::SetTy &Set : Sets) {
    if (CompatibleSets::shouldBelongToSameSet({Set.front(), II}))
      return Set;
  }

  // Otherwise, we either had no sets yet, or this call forms a new set.
  return Sets.emplace_back();
}

void CompatibleSets::insert(CallInst *II) {
  getCompatibleSet(II).emplace_back(II);
}

bool CompatibleSets::shouldBelongToSameSet(ArrayRef<CallInst *> Calls) {
  assert(Calls.size() == 2 && "Always called with exactly two candidates.");

  // Can we theoretically merge these `call`s?
  auto IsIllegalToMerge = [](CallInst *II) {
    return II->cannotMerge() || II->isInlineAsm();
  };
  if (any_of(Calls, IsIllegalToMerge))
    return false;

  // Either both `call`s must be   direct,
  // or     both `call`s must be indirect.
  auto IsIndirectCall = [](CallInst *II) { return II->isIndirectCall(); };
  bool HaveIndirectCalls = any_of(Calls, IsIndirectCall);
  bool AllCallsAreIndirect = all_of(Calls, IsIndirectCall);
  if (HaveIndirectCalls) {
    if (!AllCallsAreIndirect)
      return false;
  } else {
    // All callees must be identical.
    Value *Callee = nullptr;
    for (CallInst *II : Calls) {
      Value *CurrCallee = II->getCalledOperand();
      assert(CurrCallee && "There is always a called operand.");
      if (!Callee)
        Callee = CurrCallee;
      else if (Callee != CurrCallee)
        return false;
    }
  }

  // Ignoring arguments, these `call`s must be identical,
  // including operand bundles.
  const CallInst *II0 = Calls.front();
  for (auto *II : Calls.drop_front())
    if (!II->isSameOperationAs(II0))
      return false;

  // Can we theoretically form the data operands for the merged `call`?
  auto IsIllegalToMergeArguments = [](auto Ops) {
    Type *Ty = std::get<0>(Ops)->getType();
    assert(Ty == std::get<1>(Ops)->getType() && "Incompatible types?");
    return Ty->isTokenTy() && std::get<0>(Ops) != std::get<1>(Ops);
  };
  assert(Calls.size() == 2 && "Always called with exactly two candidates.");
  if (any_of(zip(Calls[0]->data_ops(), Calls[1]->data_ops()),
             IsIllegalToMergeArguments))
    return false;

  return true;
}

} // namespace

// Merge all calls in the provided set, all of which are compatible
// as per the `CompatibleSets::shouldBelongToSameSet()`.
static void MergeCompatibleUnreachableTerminatedCallsImpl(
    ArrayRef<CallInst *> Calls,
    std::vector<DominatorTree::UpdateType> *Updates) {
  assert(Calls.size() >= 2 && "Must have at least two calls to merge.");

  if (Updates)
    Updates->reserve(Updates->size() + Calls.size());

  // Clone one of the calls into a new basic block.
  // Since they are all compatible, it doesn't matter which call is cloned.
  CallInst *MergedCall = [&Calls]() {
    CallInst *II0 = Calls.front();
    BasicBlock *II0BB = II0->getParent();
    BasicBlock *InsertBeforeBlock =
        II0->getParent()->getIterator()->getNextNode();
    Function *Func = II0BB->getParent();
    LLVMContext &Ctx = II0->getContext();

    BasicBlock *MergedCallBB =
        BasicBlock::Create(Ctx, "", Func, InsertBeforeBlock);

    auto *MergedCall = cast<CallInst>(II0->clone());
    // NOTE: all calls have the same attributes, so no handling needed.
    MergedCallBB->getInstList().push_back(MergedCall);
    new UnreachableInst(Ctx, MergedCallBB);

    return MergedCall;
  }();

  if (Updates) {
    // Blocks that contained these calls will now branch to
    // the new block that contains the merged call.
    for (CallInst *CI : Calls)
      Updates->push_back(
          {DominatorTree::Insert, CI->getParent(), MergedCall->getParent()});
  }

  bool IsIndirectCall = Calls[0]->isIndirectCall();

  // Form the merged operands for the merged call.
  for (Use &U : MergedCall->operands()) {
    // Only PHI together the indirect callees and data operands.
    if (MergedCall->isCallee(&U)) {
      if (!IsIndirectCall)
        continue;
    } else if (!MergedCall->isDataOperand(&U))
      continue;

    // Don't create trivial PHI's with all-identical incoming values.
    bool NeedPHI = any_of(Calls, [&U](CallInst *CI) {
      return CI->getOperand(U.getOperandNo()) != U.get();
    });
    if (!NeedPHI)
      continue;

    // Form a PHI out of all the data ops under this index.
    PHINode *PN = PHINode::Create(
        U->getType(), /*NumReservedValues=*/Calls.size(), "", MergedCall);
    for (CallInst *CI : Calls)
      PN->addIncoming(CI->getOperand(U.getOperandNo()), CI->getParent());

    U.set(PN);
  }

  // And finally, replace the original `call`s with an unconditional branch
  // to the block with the merged `call`. Also, give that merged `call`
  // the merged debugloc of all the original `call`s.
  const DILocation *MergedDebugLoc = nullptr;
  for (CallInst *CI : Calls) {
    // Compute the debug location common to all the original `call`s.
    if (!MergedDebugLoc)
      MergedDebugLoc = CI->getDebugLoc();
    else
      MergedDebugLoc =
          DILocation::getMergedLocation(MergedDebugLoc, CI->getDebugLoc());

    // And replace the old `call`+`unreachable` with an unconditional branch
    // to the block with the merged `call`.
    BranchInst::Create(MergedCall->getParent(), CI->getParent());
    cast<UnreachableInst>(CI->getNextNode())->eraseFromParent();
    CI->eraseFromParent();
  }
  MergedCall->setDebugLoc(MergedDebugLoc);
}

static bool MergeCompatibleUnreachableTerminatedCalls(
    ArrayRef<BasicBlock *> BBs,
    std::vector<DominatorTree::UpdateType> *Updates) {
  bool Changed = false;

  CompatibleSets Grouper;

  for (BasicBlock *BB : BBs) {
    auto *Term = BB->getTerminator();
    assert(isa<UnreachableInst>(Term) &&
           "Only for blocks with `unreachable` terminator.");
    // Only deal with blocks where `unreachable` is preceeded by a `call`.
    if (auto *CI = dyn_cast_or_null<CallInst>(Term->getPrevNode()))
      Grouper.insert(CI);
  }

  for (ArrayRef<CallInst *> Calls : Grouper.Sets) {
    if (Calls.size() < 2)
      continue;
    Changed = true;
    MergeCompatibleUnreachableTerminatedCallsImpl(Calls, Updates);
  }

  return Changed;
}

static bool
performBlockTailMerging(Function &F, ArrayRef<BasicBlock *> BBs,
                        std::vector<DominatorTree::UpdateType> *Updates) {
  SmallVector<PHINode *, 1> NewOps;

  // We don't want to change IR just because we can.
  // Only do that if there are at least two blocks we'll tail-merge.
  if (BBs.size() < 2)
    return false;

  // Defer handling of `unreachable` blocks to the specialized utility.
  if (isa<UnreachableInst>(BBs[0]->getTerminator()))
    return MergeCompatibleUnreachableTerminatedCalls(BBs, Updates);

  if (Updates)
    Updates->reserve(Updates->size() + BBs.size());

  BasicBlock *CanonicalBB;
  Instruction *CanonicalTerm;
  {
    auto *Term = BBs[0]->getTerminator();

    // Create a canonical block for this function terminator type now,
    // placing it *before* the first block that will branch to it.
    CanonicalBB = BasicBlock::Create(
        F.getContext(), Twine("common.") + Term->getOpcodeName(), &F, BBs[0]);
    // We'll also need a PHI node per each operand of the terminator.
    NewOps.resize(Term->getNumOperands());
    for (auto I : zip(Term->operands(), NewOps)) {
      std::get<1>(I) = PHINode::Create(std::get<0>(I)->getType(),
                                       /*NumReservedValues=*/BBs.size(),
                                       CanonicalBB->getName() + ".op");
      CanonicalBB->getInstList().push_back(std::get<1>(I));
    }
    // Make it so that this canonical block actually has the right
    // terminator.
    CanonicalTerm = Term->clone();
    CanonicalBB->getInstList().push_back(CanonicalTerm);
    // If the canonical terminator has operands, rewrite it to take PHI's.
    for (auto I : zip(NewOps, CanonicalTerm->operands()))
      std::get<1>(I) = std::get<0>(I);
  }

  // Now, go through each block (with the current terminator type)
  // we've recorded, and rewrite it to branch to the new common block.
  const DILocation *CommonDebugLoc = nullptr;
  for (BasicBlock *BB : BBs) {
    auto *Term = BB->getTerminator();
    assert(Term->getOpcode() == CanonicalTerm->getOpcode() &&
           "All blocks to be tail-merged must be the same "
           "(function-terminating) terminator type.");

    // Aha, found a new non-canonical function terminator. If it has operands,
    // forward them to the PHI nodes in the canonical block.
    for (auto I : zip(Term->operands(), NewOps))
      std::get<1>(I)->addIncoming(std::get<0>(I), BB);

    // Compute the debug location common to all the original terminators.
    if (!CommonDebugLoc)
      CommonDebugLoc = Term->getDebugLoc();
    else
      CommonDebugLoc =
          DILocation::getMergedLocation(CommonDebugLoc, Term->getDebugLoc());

    // And turn BB into a block that just unconditionally branches
    // to the canonical block.
    Term->eraseFromParent();
    BranchInst::Create(CanonicalBB, BB);
    if (Updates)
      Updates->push_back({DominatorTree::Insert, BB, CanonicalBB});
  }

  CanonicalTerm->setDebugLoc(CommonDebugLoc);

  return true;
}

static bool tailMergeBlocksWithSimilarFunctionTerminators(Function &F,
                                                          DomTreeUpdater *DTU) {
  SmallMapVector<unsigned /*TerminatorOpcode*/, SmallVector<BasicBlock *, 2>, 4>
      Structure;

  // Scan all the blocks in the function, record the interesting-ones.
  for (BasicBlock &BB : F) {
    if (DTU && DTU->isBBPendingDeletion(&BB))
      continue;

    // We are only interested in function-terminating blocks.
    if (!succ_empty(&BB))
      continue;

    auto *Term = BB.getTerminator();

    // Fow now only support `ret`/`resume` function terminators.
    // FIXME: lift this restriction.
    switch (Term->getOpcode()) {
    case Instruction::Ret:
    case Instruction::Resume:
    case Instruction::Unreachable:
      break;
    default:
      continue;
    }

    // We can't tail-merge block that contains a musttail call.
    if (BB.getTerminatingMustTailCall())
      continue;

    // Calls to experimental_deoptimize must be followed by a return
    // of the value computed by experimental_deoptimize.
    // I.e., we can not change `ret` to `br` for this block.
    if (auto *CI =
            dyn_cast_or_null<CallInst>(Term->getPrevNonDebugInstruction())) {
      if (Function *F = CI->getCalledFunction())
        if (Intrinsic::ID ID = F->getIntrinsicID())
          if (ID == Intrinsic::experimental_deoptimize)
            continue;
    }

    // PHI nodes cannot have token type, so if the terminator has an operand
    // with token type, we can not tail-merge this kind of function terminators.
    if (any_of(Term->operands(),
               [](Value *Op) { return Op->getType()->isTokenTy(); }))
      continue;

    // Canonical blocks are uniqued based on the terminator type (opcode).
    Structure[Term->getOpcode()].emplace_back(&BB);
  }

  bool Changed = false;

  std::vector<DominatorTree::UpdateType> Updates;

  for (ArrayRef<BasicBlock *> BBs : make_second_range(Structure))
    Changed |= performBlockTailMerging(F, BBs, DTU ? &Updates : nullptr);

  if (DTU)
    DTU->applyUpdates(Updates);

  return Changed;
}

/// Call SimplifyCFG on all the blocks in the function,
/// iterating until no more changes are made.
static bool iterativelySimplifyCFG(Function &F, const TargetTransformInfo &TTI,
                                   DomTreeUpdater *DTU,
                                   const SimplifyCFGOptions &Options) {
  bool Changed = false;
  bool LocalChange = true;

  SmallVector<std::pair<const BasicBlock *, const BasicBlock *>, 32> Edges;
  FindFunctionBackedges(F, Edges);
  SmallPtrSet<BasicBlock *, 16> UniqueLoopHeaders;
  for (unsigned i = 0, e = Edges.size(); i != e; ++i)
    UniqueLoopHeaders.insert(const_cast<BasicBlock *>(Edges[i].second));

  SmallVector<WeakVH, 16> LoopHeaders(UniqueLoopHeaders.begin(),
                                      UniqueLoopHeaders.end());

  unsigned IterCnt = 0;
  (void)IterCnt;
  while (LocalChange) {
    assert(IterCnt++ < 1000 && "Iterative simplification didn't converge!");
    LocalChange = false;

    // Loop over all of the basic blocks and remove them if they are unneeded.
    for (Function::iterator BBIt = F.begin(); BBIt != F.end(); ) {
      BasicBlock &BB = *BBIt++;
      if (DTU) {
        assert(
            !DTU->isBBPendingDeletion(&BB) &&
            "Should not end up trying to simplify blocks marked for removal.");
        // Make sure that the advanced iterator does not point at the blocks
        // that are marked for removal, skip over all such blocks.
        while (BBIt != F.end() && DTU->isBBPendingDeletion(&*BBIt))
          ++BBIt;
      }
      if (simplifyCFG(&BB, TTI, DTU, Options, LoopHeaders)) {
        LocalChange = true;
        ++NumSimpl;
      }
    }
    Changed |= LocalChange;
  }
  return Changed;
}

static bool simplifyFunctionCFGImpl(Function &F, const TargetTransformInfo &TTI,
                                    DominatorTree *DT,
                                    const SimplifyCFGOptions &Options) {
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Eager);

  bool EverChanged = removeUnreachableBlocks(F, DT ? &DTU : nullptr);
  EverChanged |=
      tailMergeBlocksWithSimilarFunctionTerminators(F, DT ? &DTU : nullptr);
  EverChanged |= iterativelySimplifyCFG(F, TTI, DT ? &DTU : nullptr, Options);

  // If neither pass changed anything, we're done.
  if (!EverChanged) return false;

  // iterativelySimplifyCFG can (rarely) make some loops dead.  If this happens,
  // removeUnreachableBlocks is needed to nuke them, which means we should
  // iterate between the two optimizations.  We structure the code like this to
  // avoid rerunning iterativelySimplifyCFG if the second pass of
  // removeUnreachableBlocks doesn't do anything.
  if (!removeUnreachableBlocks(F, DT ? &DTU : nullptr))
    return true;

  do {
    EverChanged = iterativelySimplifyCFG(F, TTI, DT ? &DTU : nullptr, Options);
    EverChanged |= removeUnreachableBlocks(F, DT ? &DTU : nullptr);
  } while (EverChanged);

  return true;
}

static bool simplifyFunctionCFG(Function &F, const TargetTransformInfo &TTI,
                                DominatorTree *DT,
                                const SimplifyCFGOptions &Options) {
  assert((!RequireAndPreserveDomTree ||
          (DT && DT->verify(DominatorTree::VerificationLevel::Full))) &&
         "Original domtree is invalid?");

  bool Changed = simplifyFunctionCFGImpl(F, TTI, DT, Options);

  assert((!RequireAndPreserveDomTree ||
          (DT && DT->verify(DominatorTree::VerificationLevel::Full))) &&
         "Failed to maintain validity of domtree!");

  return Changed;
}

// Command-line settings override compile-time settings.
static void applyCommandLineOverridesToOptions(SimplifyCFGOptions &Options) {
  if (UserBonusInstThreshold.getNumOccurrences())
    Options.BonusInstThreshold = UserBonusInstThreshold;
  if (UserForwardSwitchCond.getNumOccurrences())
    Options.ForwardSwitchCondToPhi = UserForwardSwitchCond;
  if (UserSwitchToLookup.getNumOccurrences())
    Options.ConvertSwitchToLookupTable = UserSwitchToLookup;
  if (UserKeepLoops.getNumOccurrences())
    Options.NeedCanonicalLoop = UserKeepLoops;
  if (UserHoistCommonInsts.getNumOccurrences())
    Options.HoistCommonInsts = UserHoistCommonInsts;
  if (UserSinkCommonInsts.getNumOccurrences())
    Options.SinkCommonInsts = UserSinkCommonInsts;
}

SimplifyCFGPass::SimplifyCFGPass() {
  applyCommandLineOverridesToOptions(Options);
}

SimplifyCFGPass::SimplifyCFGPass(const SimplifyCFGOptions &Opts)
    : Options(Opts) {
  applyCommandLineOverridesToOptions(Options);
}

void SimplifyCFGPass::printPipeline(
    raw_ostream &OS, function_ref<StringRef(StringRef)> MapClassName2PassName) {
  static_cast<PassInfoMixin<SimplifyCFGPass> *>(this)->printPipeline(
      OS, MapClassName2PassName);
  OS << "<";
  OS << "bonus-inst-threshold=" << Options.BonusInstThreshold << ";";
  OS << (Options.ForwardSwitchCondToPhi ? "" : "no-") << "forward-switch-cond;";
  OS << (Options.ConvertSwitchToLookupTable ? "" : "no-")
     << "switch-to-lookup;";
  OS << (Options.NeedCanonicalLoop ? "" : "no-") << "keep-loops;";
  OS << (Options.HoistCommonInsts ? "" : "no-") << "hoist-common-insts;";
  OS << (Options.SinkCommonInsts ? "" : "no-") << "sink-common-insts";
  OS << ">";
}

PreservedAnalyses SimplifyCFGPass::run(Function &F,
                                       FunctionAnalysisManager &AM) {
  auto &TTI = AM.getResult<TargetIRAnalysis>(F);
  Options.AC = &AM.getResult<AssumptionAnalysis>(F);
  DominatorTree *DT = nullptr;
  if (RequireAndPreserveDomTree)
    DT = &AM.getResult<DominatorTreeAnalysis>(F);
  if (F.hasFnAttribute(Attribute::OptForFuzzing)) {
    Options.setSimplifyCondBranch(false).setFoldTwoEntryPHINode(false);
  } else {
    Options.setSimplifyCondBranch(true).setFoldTwoEntryPHINode(true);
  }
  if (!simplifyFunctionCFG(F, TTI, DT, Options))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  if (RequireAndPreserveDomTree)
    PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

namespace {
struct CFGSimplifyPass : public FunctionPass {
  static char ID;
  SimplifyCFGOptions Options;
  std::function<bool(const Function &)> PredicateFtor;

  CFGSimplifyPass(SimplifyCFGOptions Options_ = SimplifyCFGOptions(),
                  std::function<bool(const Function &)> Ftor = nullptr)
      : FunctionPass(ID), Options(Options_), PredicateFtor(std::move(Ftor)) {

    initializeCFGSimplifyPassPass(*PassRegistry::getPassRegistry());

    // Check for command-line overrides of options for debug/customization.
    applyCommandLineOverridesToOptions(Options);
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F) || (PredicateFtor && !PredicateFtor(F)))
      return false;

    Options.AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
    DominatorTree *DT = nullptr;
    if (RequireAndPreserveDomTree)
      DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    if (F.hasFnAttribute(Attribute::OptForFuzzing)) {
      Options.setSimplifyCondBranch(false)
             .setFoldTwoEntryPHINode(false);
    } else {
      Options.setSimplifyCondBranch(true)
             .setFoldTwoEntryPHINode(true);
    }

    auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
    return simplifyFunctionCFG(F, TTI, DT, Options);
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AssumptionCacheTracker>();
    if (RequireAndPreserveDomTree)
      AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    if (RequireAndPreserveDomTree)
      AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
  }
};
}

char CFGSimplifyPass::ID = 0;
INITIALIZE_PASS_BEGIN(CFGSimplifyPass, "simplifycfg", "Simplify the CFG", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(CFGSimplifyPass, "simplifycfg", "Simplify the CFG", false,
                    false)

// Public interface to the CFGSimplification pass
FunctionPass *
llvm::createCFGSimplificationPass(SimplifyCFGOptions Options,
                                  std::function<bool(const Function &)> Ftor) {
  return new CFGSimplifyPass(Options, std::move(Ftor));
}
