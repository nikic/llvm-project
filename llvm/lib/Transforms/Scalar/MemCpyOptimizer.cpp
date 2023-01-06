//===- MemCpyOptimizer.cpp - Optimize use of memcpy and friends -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass performs various transformations related to eliminating memcpy
// calls, or transforming sets of stores into memset's.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/MemCpyOptimizer.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/Bitfields.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/Loads.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "memcpyopt"

static cl::opt<bool> EnableMemCpyOptWithoutLibcalls(
    "enable-memcpyopt-without-libcalls", cl::Hidden,
    cl::desc("Enable memcpyopt even when libcalls are disabled"));
static cl::opt<unsigned>
    MemCpyOptStackMoveThreshold("memcpyopt-stack-move-threshold", cl::Hidden,
                                cl::desc("Maximum number of basic blocks the "
                                         "stack-move optimization may examine"),
                                cl::init(250));

STATISTIC(NumMemCpyInstr, "Number of memcpy instructions deleted");
STATISTIC(NumMemSetInfer, "Number of memsets inferred");
STATISTIC(NumMoveToCpy,   "Number of memmoves converted to memcpy");
STATISTIC(NumCpyToSet,    "Number of memcpys converted to memset");
STATISTIC(NumCallSlot,    "Number of call slot optimizations performed");
STATISTIC(NumStackMove, "Number of stack-move optimizations performed");

namespace {

/// Represents a range of memset'd bytes with the ByteVal value.
/// This allows us to analyze stores like:
///   store 0 -> P+1
///   store 0 -> P+0
///   store 0 -> P+3
///   store 0 -> P+2
/// which sometimes happens with stores to arrays of structs etc.  When we see
/// the first store, we make a range [1, 2).  The second store extends the range
/// to [0, 2).  The third makes a new range [2, 3).  The fourth store joins the
/// two ranges into [0, 3) which is memset'able.
struct MemsetRange {
  // Start/End - A semi range that describes the span that this range covers.
  // The range is closed at the start and open at the end: [Start, End).
  int64_t Start, End;

  /// StartPtr - The getelementptr instruction that points to the start of the
  /// range.
  Value *StartPtr;

  /// Alignment - The known alignment of the first store.
  MaybeAlign Alignment;

  /// TheStores - The actual stores that make up this range.
  SmallVector<Instruction*, 16> TheStores;

  bool isProfitableToUseMemset(const DataLayout &DL) const;
};

} // end anonymous namespace

bool MemsetRange::isProfitableToUseMemset(const DataLayout &DL) const {
  // If we found more than 4 stores to merge or 16 bytes, use memset.
  if (TheStores.size() >= 4 || End-Start >= 16) return true;

  // If there is nothing to merge, don't do anything.
  if (TheStores.size() < 2) return false;

  // If any of the stores are a memset, then it is always good to extend the
  // memset.
  for (Instruction *SI : TheStores)
    if (!isa<StoreInst>(SI))
      return true;

  // Assume that the code generator is capable of merging pairs of stores
  // together if it wants to.
  if (TheStores.size() == 2) return false;

  // If we have fewer than 8 stores, it can still be worthwhile to do this.
  // For example, merging 4 i8 stores into an i32 store is useful almost always.
  // However, merging 2 32-bit stores isn't useful on a 32-bit architecture (the
  // memset will be split into 2 32-bit stores anyway) and doing so can
  // pessimize the llvm optimizer.
  //
  // Since we don't have perfect knowledge here, make some assumptions: assume
  // the maximum GPR width is the same size as the largest legal integer
  // size. If so, check to see whether we will end up actually reducing the
  // number of stores used.
  unsigned Bytes = unsigned(End-Start);
  unsigned MaxIntSize = DL.getLargestLegalIntTypeSizeInBits() / 8;
  if (MaxIntSize == 0)
    MaxIntSize = 1;
  unsigned NumPointerStores = Bytes / MaxIntSize;

  // Assume the remaining bytes if any are done a byte at a time.
  unsigned NumByteStores = Bytes % MaxIntSize;

  // If we will reduce the # stores (according to this heuristic), do the
  // transformation.  This encourages merging 4 x i8 -> i32 and 2 x i16 -> i32
  // etc.
  return TheStores.size() > NumPointerStores+NumByteStores;
}

namespace {

class MemsetRanges {
  using range_iterator = SmallVectorImpl<MemsetRange>::iterator;

  /// A sorted list of the memset ranges.
  SmallVector<MemsetRange, 8> Ranges;

  const DataLayout &DL;

public:
  MemsetRanges(const DataLayout &DL) : DL(DL) {}

  using const_iterator = SmallVectorImpl<MemsetRange>::const_iterator;

  const_iterator begin() const { return Ranges.begin(); }
  const_iterator end() const { return Ranges.end(); }
  bool empty() const { return Ranges.empty(); }

  void addInst(int64_t OffsetFromFirst, Instruction *Inst) {
    if (auto *SI = dyn_cast<StoreInst>(Inst))
      addStore(OffsetFromFirst, SI);
    else
      addMemSet(OffsetFromFirst, cast<MemSetInst>(Inst));
  }

  void addStore(int64_t OffsetFromFirst, StoreInst *SI) {
    TypeSize StoreSize = DL.getTypeStoreSize(SI->getOperand(0)->getType());
    assert(!StoreSize.isScalable() && "Can't track scalable-typed stores");
    addRange(OffsetFromFirst, StoreSize.getFixedSize(), SI->getPointerOperand(),
             SI->getAlign(), SI);
  }

  void addMemSet(int64_t OffsetFromFirst, MemSetInst *MSI) {
    int64_t Size = cast<ConstantInt>(MSI->getLength())->getZExtValue();
    addRange(OffsetFromFirst, Size, MSI->getDest(), MSI->getDestAlign(), MSI);
  }

  void addRange(int64_t Start, int64_t Size, Value *Ptr, MaybeAlign Alignment,
                Instruction *Inst);
};

} // end anonymous namespace

/// Add a new store to the MemsetRanges data structure.  This adds a
/// new range for the specified store at the specified offset, merging into
/// existing ranges as appropriate.
void MemsetRanges::addRange(int64_t Start, int64_t Size, Value *Ptr,
                            MaybeAlign Alignment, Instruction *Inst) {
  int64_t End = Start+Size;

  range_iterator I = partition_point(
      Ranges, [=](const MemsetRange &O) { return O.End < Start; });

  // We now know that I == E, in which case we didn't find anything to merge
  // with, or that Start <= I->End.  If End < I->Start or I == E, then we need
  // to insert a new range.  Handle this now.
  if (I == Ranges.end() || End < I->Start) {
    MemsetRange &R = *Ranges.insert(I, MemsetRange());
    R.Start        = Start;
    R.End          = End;
    R.StartPtr     = Ptr;
    R.Alignment    = Alignment;
    R.TheStores.push_back(Inst);
    return;
  }

  // This store overlaps with I, add it.
  I->TheStores.push_back(Inst);

  // At this point, we may have an interval that completely contains our store.
  // If so, just add it to the interval and return.
  if (I->Start <= Start && I->End >= End)
    return;

  // Now we know that Start <= I->End and End >= I->Start so the range overlaps
  // but is not entirely contained within the range.

  // See if the range extends the start of the range.  In this case, it couldn't
  // possibly cause it to join the prior range, because otherwise we would have
  // stopped on *it*.
  if (Start < I->Start) {
    I->Start = Start;
    I->StartPtr = Ptr;
    I->Alignment = Alignment;
  }

  // Now we know that Start <= I->End and Start >= I->Start (so the startpoint
  // is in or right at the end of I), and that End >= I->Start.  Extend I out to
  // End.
  if (End > I->End) {
    I->End = End;
    range_iterator NextI = I;
    while (++NextI != Ranges.end() && End >= NextI->Start) {
      // Merge the range in.
      I->TheStores.append(NextI->TheStores.begin(), NextI->TheStores.end());
      if (NextI->End > I->End)
        I->End = NextI->End;
      Ranges.erase(NextI);
      NextI = I;
    }
  }
}

//===----------------------------------------------------------------------===//
//                         MemCpyOptLegacyPass Pass
//===----------------------------------------------------------------------===//

namespace {

class MemCpyOptLegacyPass : public FunctionPass {
  MemCpyOptPass Impl;

public:
  static char ID; // Pass identification, replacement for typeid

  MemCpyOptLegacyPass() : FunctionPass(ID) {
    initializeMemCpyOptLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

private:
  // This transformation requires dominator postdominator info
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addPreserved<PostDominatorTreeWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addPreserved<AAResultsWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
    AU.addPreserved<MemorySSAWrapperPass>();
  }
};

} // end anonymous namespace

char MemCpyOptLegacyPass::ID = 0;

/// The public interface to this file...
FunctionPass *llvm::createMemCpyOptPass() { return new MemCpyOptLegacyPass(); }

INITIALIZE_PASS_BEGIN(MemCpyOptLegacyPass, "memcpyopt", "MemCpy Optimization",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(GlobalsAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MemorySSAWrapperPass)
INITIALIZE_PASS_END(MemCpyOptLegacyPass, "memcpyopt", "MemCpy Optimization",
                    false, false)

// Check that V is either not accessible by the caller, or unwinding cannot
// occur between Start and End.
static bool mayBeVisibleThroughUnwinding(Value *V, Instruction *Start,
                                         Instruction *End) {
  assert(Start->getParent() == End->getParent() && "Must be in same block");
  // Function can't unwind, so it also can't be visible through unwinding.
  if (Start->getFunction()->doesNotThrow())
    return false;

  // Object is not visible on unwind.
  // TODO: Support RequiresNoCaptureBeforeUnwind case.
  bool RequiresNoCaptureBeforeUnwind;
  if (isNotVisibleOnUnwind(getUnderlyingObject(V),
                           RequiresNoCaptureBeforeUnwind) &&
      !RequiresNoCaptureBeforeUnwind)
    return false;

  // Check whether there are any unwinding instructions in the range.
  return any_of(make_range(Start->getIterator(), End->getIterator()),
                [](const Instruction &I) { return I.mayThrow(); });
}

void MemCpyOptPass::eraseInstruction(Instruction *I) {
  MSSAU->removeMemoryAccess(I);
  I->eraseFromParent();
}

// Check for mod or ref of Loc between Start and End, excluding both boundaries.
// Start and End must be in the same block.
// If SkippedLifetimeStart is provided, skip over one clobbering lifetime.start
// intrinsic and store it inside SkippedLifetimeStart.
static bool accessedBetween(BatchAAResults &AA, MemoryLocation Loc,
                            const MemoryUseOrDef *Start,
                            const MemoryUseOrDef *End,
                            Instruction **SkippedLifetimeStart = nullptr) {
  assert(Start->getBlock() == End->getBlock() && "Only local supported");
  for (const MemoryAccess &MA :
       make_range(++Start->getIterator(), End->getIterator())) {
    Instruction *I = cast<MemoryUseOrDef>(MA).getMemoryInst();
    if (isModOrRefSet(AA.getModRefInfo(I, Loc))) {
      auto *II = dyn_cast<IntrinsicInst>(I);
      if (II && II->getIntrinsicID() == Intrinsic::lifetime_start &&
          SkippedLifetimeStart && !*SkippedLifetimeStart) {
        *SkippedLifetimeStart = I;
        continue;
      }

      return true;
    }
  }
  return false;
}

// Check for mod of Loc between Start and End, excluding both boundaries.
// Start and End can be in different blocks.
static bool writtenBetween(MemorySSA *MSSA, BatchAAResults &AA,
                           MemoryLocation Loc, const MemoryUseOrDef *Start,
                           const MemoryUseOrDef *End) {
  if (isa<MemoryUse>(End)) {
    // For MemoryUses, getClobberingMemoryAccess may skip non-clobbering writes.
    // Manually check read accesses between Start and End, if they are in the
    // same block, for clobbers. Otherwise assume Loc is clobbered.
    return Start->getBlock() != End->getBlock() ||
           any_of(
               make_range(std::next(Start->getIterator()), End->getIterator()),
               [&AA, Loc](const MemoryAccess &Acc) {
                 if (isa<MemoryUse>(&Acc))
                   return false;
                 Instruction *AccInst =
                     cast<MemoryUseOrDef>(&Acc)->getMemoryInst();
                 return isModSet(AA.getModRefInfo(AccInst, Loc));
               });
  }

  // TODO: Only walk until we hit Start.
  MemoryAccess *Clobber = MSSA->getWalker()->getClobberingMemoryAccess(
      End->getDefiningAccess(), Loc, AA);
  return !MSSA->dominates(Clobber, Start);
}

/// When scanning forward over instructions, we look for some other patterns to
/// fold away. In particular, this looks for stores to neighboring locations of
/// memory. If it sees enough consecutive ones, it attempts to merge them
/// together into a memcpy/memset.
Instruction *MemCpyOptPass::tryMergingIntoMemset(Instruction *StartInst,
                                                 Value *StartPtr,
                                                 Value *ByteVal) {
  const DataLayout &DL = StartInst->getModule()->getDataLayout();

  // We can't track scalable types
  if (auto *SI = dyn_cast<StoreInst>(StartInst))
    if (DL.getTypeStoreSize(SI->getOperand(0)->getType()).isScalable())
      return nullptr;

  // Okay, so we now have a single store that can be splatable.  Scan to find
  // all subsequent stores of the same value to offset from the same pointer.
  // Join these together into ranges, so we can decide whether contiguous blocks
  // are stored.
  MemsetRanges Ranges(DL);

  BasicBlock::iterator BI(StartInst);

  // Keeps track of the last memory use or def before the insertion point for
  // the new memset. The new MemoryDef for the inserted memsets will be inserted
  // after MemInsertPoint. It points to either LastMemDef or to the last user
  // before the insertion point of the memset, if there are any such users.
  MemoryUseOrDef *MemInsertPoint = nullptr;
  // Keeps track of the last MemoryDef between StartInst and the insertion point
  // for the new memset. This will become the defining access of the inserted
  // memsets.
  MemoryDef *LastMemDef = nullptr;
  for (++BI; !BI->isTerminator(); ++BI) {
    auto *CurrentAcc = cast_or_null<MemoryUseOrDef>(
        MSSAU->getMemorySSA()->getMemoryAccess(&*BI));
    if (CurrentAcc) {
      MemInsertPoint = CurrentAcc;
      if (auto *CurrentDef = dyn_cast<MemoryDef>(CurrentAcc))
        LastMemDef = CurrentDef;
    }

    // Calls that only access inaccessible memory do not block merging
    // accessible stores.
    if (auto *CB = dyn_cast<CallBase>(BI)) {
      if (CB->onlyAccessesInaccessibleMemory())
        continue;
    }

    if (!isa<StoreInst>(BI) && !isa<MemSetInst>(BI)) {
      // If the instruction is readnone, ignore it, otherwise bail out.  We
      // don't even allow readonly here because we don't want something like:
      // A[1] = 2; strlen(A); A[2] = 2; -> memcpy(A, ...); strlen(A).
      if (BI->mayWriteToMemory() || BI->mayReadFromMemory())
        break;
      continue;
    }

    if (auto *NextStore = dyn_cast<StoreInst>(BI)) {
      // If this is a store, see if we can merge it in.
      if (!NextStore->isSimple()) break;

      Value *StoredVal = NextStore->getValueOperand();

      // Don't convert stores of non-integral pointer types to memsets (which
      // stores integers).
      if (DL.isNonIntegralPointerType(StoredVal->getType()->getScalarType()))
        break;

      // We can't track ranges involving scalable types.
      if (DL.getTypeStoreSize(StoredVal->getType()).isScalable())
        break;

      // Check to see if this stored value is of the same byte-splattable value.
      Value *StoredByte = isBytewiseValue(StoredVal, DL);
      if (isa<UndefValue>(ByteVal) && StoredByte)
        ByteVal = StoredByte;
      if (ByteVal != StoredByte)
        break;

      // Check to see if this store is to a constant offset from the start ptr.
      std::optional<int64_t> Offset =
          isPointerOffset(StartPtr, NextStore->getPointerOperand(), DL);
      if (!Offset)
        break;

      Ranges.addStore(*Offset, NextStore);
    } else {
      auto *MSI = cast<MemSetInst>(BI);

      if (MSI->isVolatile() || ByteVal != MSI->getValue() ||
          !isa<ConstantInt>(MSI->getLength()))
        break;

      // Check to see if this store is to a constant offset from the start ptr.
      std::optional<int64_t> Offset =
          isPointerOffset(StartPtr, MSI->getDest(), DL);
      if (!Offset)
        break;

      Ranges.addMemSet(*Offset, MSI);
    }
  }

  // If we have no ranges, then we just had a single store with nothing that
  // could be merged in.  This is a very common case of course.
  if (Ranges.empty())
    return nullptr;

  // If we had at least one store that could be merged in, add the starting
  // store as well.  We try to avoid this unless there is at least something
  // interesting as a small compile-time optimization.
  Ranges.addInst(0, StartInst);

  // If we create any memsets, we put it right before the first instruction that
  // isn't part of the memset block.  This ensure that the memset is dominated
  // by any addressing instruction needed by the start of the block.
  IRBuilder<> Builder(&*BI);

  // Now that we have full information about ranges, loop over the ranges and
  // emit memset's for anything big enough to be worthwhile.
  Instruction *AMemSet = nullptr;
  for (const MemsetRange &Range : Ranges) {
    if (Range.TheStores.size() == 1) continue;

    // If it is profitable to lower this range to memset, do so now.
    if (!Range.isProfitableToUseMemset(DL))
      continue;

    // Otherwise, we do want to transform this!  Create a new memset.
    // Get the starting pointer of the block.
    StartPtr = Range.StartPtr;

    AMemSet = Builder.CreateMemSet(StartPtr, ByteVal, Range.End - Range.Start,
                                   Range.Alignment);
    AMemSet->mergeDIAssignID(Range.TheStores);

    LLVM_DEBUG(dbgs() << "Replace stores:\n"; for (Instruction *SI
                                                   : Range.TheStores) dbgs()
                                              << *SI << '\n';
               dbgs() << "With: " << *AMemSet << '\n');
    if (!Range.TheStores.empty())
      AMemSet->setDebugLoc(Range.TheStores[0]->getDebugLoc());

    assert(LastMemDef && MemInsertPoint &&
           "Both LastMemDef and MemInsertPoint need to be set");
    auto *NewDef =
        cast<MemoryDef>(MemInsertPoint->getMemoryInst() == &*BI
                            ? MSSAU->createMemoryAccessBefore(
                                  AMemSet, LastMemDef, MemInsertPoint)
                            : MSSAU->createMemoryAccessAfter(
                                  AMemSet, LastMemDef, MemInsertPoint));
    MSSAU->insertDef(NewDef, /*RenameUses=*/true);
    LastMemDef = NewDef;
    MemInsertPoint = NewDef;

    // Zap all the stores.
    for (Instruction *SI : Range.TheStores)
      eraseInstruction(SI);

    ++NumMemSetInfer;
  }

  return AMemSet;
}

// This method try to lift a store instruction before position P.
// It will lift the store and its argument + that anything that
// may alias with these.
// The method returns true if it was successful.
bool MemCpyOptPass::moveUp(StoreInst *SI, Instruction *P, const LoadInst *LI) {
  // If the store alias this position, early bail out.
  MemoryLocation StoreLoc = MemoryLocation::get(SI);
  if (isModOrRefSet(AA->getModRefInfo(P, StoreLoc)))
    return false;

  // Keep track of the arguments of all instruction we plan to lift
  // so we can make sure to lift them as well if appropriate.
  DenseSet<Instruction*> Args;
  auto AddArg = [&](Value *Arg) {
    auto *I = dyn_cast<Instruction>(Arg);
    if (I && I->getParent() == SI->getParent()) {
      // Cannot hoist user of P above P
      if (I == P) return false;
      Args.insert(I);
    }
    return true;
  };
  if (!AddArg(SI->getPointerOperand()))
    return false;

  // Instruction to lift before P.
  SmallVector<Instruction *, 8> ToLift{SI};

  // Memory locations of lifted instructions.
  SmallVector<MemoryLocation, 8> MemLocs{StoreLoc};

  // Lifted calls.
  SmallVector<const CallBase *, 8> Calls;

  const MemoryLocation LoadLoc = MemoryLocation::get(LI);

  for (auto I = --SI->getIterator(), E = P->getIterator(); I != E; --I) {
    auto *C = &*I;

    // Make sure hoisting does not perform a store that was not guaranteed to
    // happen.
    if (!isGuaranteedToTransferExecutionToSuccessor(C))
      return false;

    bool MayAlias = isModOrRefSet(AA->getModRefInfo(C, std::nullopt));

    bool NeedLift = false;
    if (Args.erase(C))
      NeedLift = true;
    else if (MayAlias) {
      NeedLift = llvm::any_of(MemLocs, [C, this](const MemoryLocation &ML) {
        return isModOrRefSet(AA->getModRefInfo(C, ML));
      });

      if (!NeedLift)
        NeedLift = llvm::any_of(Calls, [C, this](const CallBase *Call) {
          return isModOrRefSet(AA->getModRefInfo(C, Call));
        });
    }

    if (!NeedLift)
      continue;

    if (MayAlias) {
      // Since LI is implicitly moved downwards past the lifted instructions,
      // none of them may modify its source.
      if (isModSet(AA->getModRefInfo(C, LoadLoc)))
        return false;
      else if (const auto *Call = dyn_cast<CallBase>(C)) {
        // If we can't lift this before P, it's game over.
        if (isModOrRefSet(AA->getModRefInfo(P, Call)))
          return false;

        Calls.push_back(Call);
      } else if (isa<LoadInst>(C) || isa<StoreInst>(C) || isa<VAArgInst>(C)) {
        // If we can't lift this before P, it's game over.
        auto ML = MemoryLocation::get(C);
        if (isModOrRefSet(AA->getModRefInfo(P, ML)))
          return false;

        MemLocs.push_back(ML);
      } else
        // We don't know how to lift this instruction.
        return false;
    }

    ToLift.push_back(C);
    for (Value *Op : C->operands())
      if (!AddArg(Op))
        return false;
  }

  // Find MSSA insertion point. Normally P will always have a corresponding
  // memory access before which we can insert. However, with non-standard AA
  // pipelines, there may be a mismatch between AA and MSSA, in which case we
  // will scan for a memory access before P. In either case, we know for sure
  // that at least the load will have a memory access.
  // TODO: Simplify this once P will be determined by MSSA, in which case the
  // discrepancy can no longer occur.
  MemoryUseOrDef *MemInsertPoint = nullptr;
  if (MemoryUseOrDef *MA = MSSAU->getMemorySSA()->getMemoryAccess(P)) {
    MemInsertPoint = cast<MemoryUseOrDef>(--MA->getIterator());
  } else {
    const Instruction *ConstP = P;
    for (const Instruction &I : make_range(++ConstP->getReverseIterator(),
                                           ++LI->getReverseIterator())) {
      if (MemoryUseOrDef *MA = MSSAU->getMemorySSA()->getMemoryAccess(&I)) {
        MemInsertPoint = MA;
        break;
      }
    }
  }

  // We made it, we need to lift.
  for (auto *I : llvm::reverse(ToLift)) {
    LLVM_DEBUG(dbgs() << "Lifting " << *I << " before " << *P << "\n");
    I->moveBefore(P);
    assert(MemInsertPoint && "Must have found insert point");
    if (MemoryUseOrDef *MA = MSSAU->getMemorySSA()->getMemoryAccess(I)) {
      MSSAU->moveAfter(MA, MemInsertPoint);
      MemInsertPoint = MA;
    }
  }

  return true;
}

bool MemCpyOptPass::processStore(StoreInst *SI, BasicBlock::iterator &BBI) {
  if (!SI->isSimple()) return false;

  // Avoid merging nontemporal stores since the resulting
  // memcpy/memset would not be able to preserve the nontemporal hint.
  // In theory we could teach how to propagate the !nontemporal metadata to
  // memset calls. However, that change would force the backend to
  // conservatively expand !nontemporal memset calls back to sequences of
  // store instructions (effectively undoing the merging).
  if (SI->getMetadata(LLVMContext::MD_nontemporal))
    return false;

  const DataLayout &DL = SI->getModule()->getDataLayout();

  Value *StoredVal = SI->getValueOperand();

  // Not all the transforms below are correct for non-integral pointers, bail
  // until we've audited the individual pieces.
  if (DL.isNonIntegralPointerType(StoredVal->getType()->getScalarType()))
    return false;

  // Load to store forwarding can be interpreted as memcpy.
  if (auto *LI = dyn_cast<LoadInst>(StoredVal)) {
    if (LI->isSimple() && LI->hasOneUse() &&
        LI->getParent() == SI->getParent()) {

      auto *T = LI->getType();
      // Don't introduce calls to memcpy/memmove intrinsics out of thin air if
      // the corresponding libcalls are not available.
      // TODO: We should really distinguish between libcall availability and
      // our ability to introduce intrinsics.
      if (T->isAggregateType() &&
          (EnableMemCpyOptWithoutLibcalls ||
           (TLI->has(LibFunc_memcpy) && TLI->has(LibFunc_memmove)))) {
        MemoryLocation LoadLoc = MemoryLocation::get(LI);

        // We use alias analysis to check if an instruction may store to
        // the memory we load from in between the load and the store. If
        // such an instruction is found, we try to promote there instead
        // of at the store position.
        // TODO: Can use MSSA for this.
        Instruction *P = SI;
        for (auto &I : make_range(++LI->getIterator(), SI->getIterator())) {
          if (isModSet(AA->getModRefInfo(&I, LoadLoc))) {
            P = &I;
            break;
          }
        }

        // We found an instruction that may write to the loaded memory.
        // We can try to promote at this position instead of the store
        // position if nothing aliases the store memory after this and the store
        // destination is not in the range.
        if (P && P != SI) {
          if (!moveUp(SI, P, LI))
            P = nullptr;
        }

        // If a valid insertion position is found, then we can promote
        // the load/store pair to a memcpy.
        if (P) {
          // If we load from memory that may alias the memory we store to,
          // memmove must be used to preserve semantic. If not, memcpy can
          // be used. Also, if we load from constant memory, memcpy can be used
          // as the constant memory won't be modified.
          bool UseMemMove = false;
          if (isModSet(AA->getModRefInfo(SI, LoadLoc)))
            UseMemMove = true;

          uint64_t Size = DL.getTypeStoreSize(T);

          IRBuilder<> Builder(P);
          Instruction *M;
          if (UseMemMove)
            M = Builder.CreateMemMove(
                SI->getPointerOperand(), SI->getAlign(),
                LI->getPointerOperand(), LI->getAlign(), Size);
          else
            M = Builder.CreateMemCpy(
                SI->getPointerOperand(), SI->getAlign(),
                LI->getPointerOperand(), LI->getAlign(), Size);
          M->copyMetadata(*SI, LLVMContext::MD_DIAssignID);

          LLVM_DEBUG(dbgs() << "Promoting " << *LI << " to " << *SI << " => "
                            << *M << "\n");

          auto *LastDef =
              cast<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(SI));
          auto *NewAccess = MSSAU->createMemoryAccessAfter(M, LastDef, LastDef);
          MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/true);

          eraseInstruction(SI);
          eraseInstruction(LI);
          ++NumMemCpyInstr;

          // Make sure we do not invalidate the iterator.
          BBI = M->getIterator();
          return true;
        }
      }

      // Detect cases where we're performing call slot forwarding, but
      // happen to be using a load-store pair to implement it, rather than
      // a memcpy.
      BatchAAResults BAA(*AA);
      auto GetCall = [&]() -> CallInst * {
        // We defer this expensive clobber walk until the cheap checks
        // have been done on the source inside performCallSlotOptzn.
        if (auto *LoadClobber = dyn_cast<MemoryUseOrDef>(
                MSSA->getWalker()->getClobberingMemoryAccess(LI, BAA)))
          return dyn_cast_or_null<CallInst>(LoadClobber->getMemoryInst());
        return nullptr;
      };

      bool changed = performCallSlotOptzn(
          LI, SI, SI->getPointerOperand()->stripPointerCasts(),
          LI->getPointerOperand()->stripPointerCasts(),
          DL.getTypeStoreSize(SI->getOperand(0)->getType()),
          std::min(SI->getAlign(), LI->getAlign()), BAA, GetCall);
      if (changed) {
        eraseInstruction(SI);
        eraseInstruction(LI);
        ++NumMemCpyInstr;
        return true;
      }

      // If this is a load-store pair from a stack slot to a stack slot, we
      // might be able to perform the stack-move optimization just as we do for
      // memcpys from an alloca to an alloca.
      if (AllocaInst *DestAlloca =
              dyn_cast<AllocaInst>(SI->getPointerOperand())) {
        if (AllocaInst *SrcAlloca =
                dyn_cast<AllocaInst>(LI->getPointerOperand())) {
          if (performStackMoveOptzn(LI, SI, DestAlloca, SrcAlloca,
                                    DL.getTypeStoreSize(T))) {
            // Avoid invalidating the iterator.
            BBI = SI->getNextNonDebugInstruction()->getIterator();
            eraseInstruction(SI);
            eraseInstruction(LI);
            ++NumMemCpyInstr;
            return true;
          }
        }
      }
    }
  }

  // The following code creates memset intrinsics out of thin air. Don't do
  // this if the corresponding libfunc is not available.
  // TODO: We should really distinguish between libcall availability and
  // our ability to introduce intrinsics.
  if (!(TLI->has(LibFunc_memset) || EnableMemCpyOptWithoutLibcalls))
    return false;

  // There are two cases that are interesting for this code to handle: memcpy
  // and memset.  Right now we only handle memset.

  // Ensure that the value being stored is something that can be memset'able a
  // byte at a time like "0" or "-1" or any width, as well as things like
  // 0xA0A0A0A0 and 0.0.
  auto *V = SI->getOperand(0);
  if (Value *ByteVal = isBytewiseValue(V, DL)) {
    if (Instruction *I = tryMergingIntoMemset(SI, SI->getPointerOperand(),
                                              ByteVal)) {
      BBI = I->getIterator(); // Don't invalidate iterator.
      return true;
    }

    // If we have an aggregate, we try to promote it to memset regardless
    // of opportunity for merging as it can expose optimization opportunities
    // in subsequent passes.
    auto *T = V->getType();
    if (T->isAggregateType()) {
      uint64_t Size = DL.getTypeStoreSize(T);
      IRBuilder<> Builder(SI);
      auto *M = Builder.CreateMemSet(SI->getPointerOperand(), ByteVal, Size,
                                     SI->getAlign());
      M->copyMetadata(*SI, LLVMContext::MD_DIAssignID);

      LLVM_DEBUG(dbgs() << "Promoting " << *SI << " to " << *M << "\n");

      // The newly inserted memset is immediately overwritten by the original
      // store, so we do not need to rename uses.
      auto *StoreDef = cast<MemoryDef>(MSSA->getMemoryAccess(SI));
      auto *NewAccess = MSSAU->createMemoryAccessBefore(
          M, StoreDef->getDefiningAccess(), StoreDef);
      MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/false);

      eraseInstruction(SI);
      NumMemSetInfer++;

      // Make sure we do not invalidate the iterator.
      BBI = M->getIterator();
      return true;
    }
  }

  return false;
}

bool MemCpyOptPass::processMemSet(MemSetInst *MSI, BasicBlock::iterator &BBI) {
  // See if there is another memset or store neighboring this memset which
  // allows us to widen out the memset to do a single larger store.
  if (isa<ConstantInt>(MSI->getLength()) && !MSI->isVolatile())
    if (Instruction *I = tryMergingIntoMemset(MSI, MSI->getDest(),
                                              MSI->getValue())) {
      BBI = I->getIterator(); // Don't invalidate iterator.
      return true;
    }
  return false;
}

/// Takes a memcpy and a call that it depends on,
/// and checks for the possibility of a call slot optimization by having
/// the call write its result directly into the destination of the memcpy.
bool MemCpyOptPass::performCallSlotOptzn(Instruction *cpyLoad,
                                         Instruction *cpyStore, Value *cpyDest,
                                         Value *cpySrc, TypeSize cpySize,
                                         Align cpyDestAlign, BatchAAResults &BAA,
                                         std::function<CallInst *()> GetC) {
  // The general transformation to keep in mind is
  //
  //   call @func(..., src, ...)
  //   memcpy(dest, src, ...)
  //
  // ->
  //
  //   memcpy(dest, src, ...)
  //   call @func(..., dest, ...)
  //
  // Since moving the memcpy is technically awkward, we additionally check that
  // src only holds uninitialized values at the moment of the call, meaning that
  // the memcpy can be discarded rather than moved.

  // We can't optimize scalable types.
  if (cpySize.isScalable())
    return false;

  // Require that src be an alloca.  This simplifies the reasoning considerably.
  auto *srcAlloca = dyn_cast<AllocaInst>(cpySrc);
  if (!srcAlloca)
    return false;

  ConstantInt *srcArraySize = dyn_cast<ConstantInt>(srcAlloca->getArraySize());
  if (!srcArraySize)
    return false;

  const DataLayout &DL = cpyLoad->getModule()->getDataLayout();
  uint64_t srcSize = DL.getTypeAllocSize(srcAlloca->getAllocatedType()) *
                     srcArraySize->getZExtValue();

  if (cpySize < srcSize)
    return false;

  CallInst *C = GetC();
  if (!C)
    return false;

  // Lifetime marks shouldn't be operated on.
  if (Function *F = C->getCalledFunction())
    if (F->isIntrinsic() && F->getIntrinsicID() == Intrinsic::lifetime_start)
      return false;


  if (C->getParent() != cpyStore->getParent()) {
    LLVM_DEBUG(dbgs() << "Call Slot: block local restriction\n");
    return false;
  }

  MemoryLocation DestLoc = isa<StoreInst>(cpyStore) ?
    MemoryLocation::get(cpyStore) :
    MemoryLocation::getForDest(cast<MemCpyInst>(cpyStore));

  // Check that nothing touches the dest of the copy between
  // the call and the store/memcpy.
  Instruction *SkippedLifetimeStart = nullptr;
  if (accessedBetween(BAA, DestLoc, MSSA->getMemoryAccess(C),
                      MSSA->getMemoryAccess(cpyStore), &SkippedLifetimeStart)) {
    LLVM_DEBUG(dbgs() << "Call Slot: Dest pointer modified after call\n");
    return false;
  }

  // If we need to move a lifetime.start above the call, make sure that we can
  // actually do so. If the argument is bitcasted for example, we would have to
  // move the bitcast as well, which we don't handle.
  if (SkippedLifetimeStart) {
    auto *LifetimeArg =
        dyn_cast<Instruction>(SkippedLifetimeStart->getOperand(1));
    if (LifetimeArg && LifetimeArg->getParent() == C->getParent() &&
        C->comesBefore(LifetimeArg))
      return false;
  }

  // Check that accessing the first srcSize bytes of dest will not cause a
  // trap.  Otherwise the transform is invalid since it might cause a trap
  // to occur earlier than it otherwise would.
  if (!isDereferenceableAndAlignedPointer(cpyDest, Align(1), APInt(64, cpySize),
                                          DL, C, AC, DT)) {
    LLVM_DEBUG(dbgs() << "Call Slot: Dest pointer not dereferenceable\n");
    return false;
  }

  // Make sure that nothing can observe cpyDest being written early. There are
  // a number of cases to consider:
  //  1. cpyDest cannot be accessed between C and cpyStore as a precondition of
  //     the transform.
  //  2. C itself may not access cpyDest (prior to the transform). This is
  //     checked further below.
  //  3. If cpyDest is accessible to the caller of this function (potentially
  //     captured and not based on an alloca), we need to ensure that we cannot
  //     unwind between C and cpyStore. This is checked here.
  //  4. If cpyDest is potentially captured, there may be accesses to it from
  //     another thread. In this case, we need to check that cpyStore is
  //     guaranteed to be executed if C is. As it is a non-atomic access, it
  //     renders accesses from other threads undefined.
  //     TODO: This is currently not checked.
  if (mayBeVisibleThroughUnwinding(cpyDest, C, cpyStore)) {
    LLVM_DEBUG(dbgs() << "Call Slot: Dest may be visible through unwinding\n");
    return false;
  }

  // Check that dest points to memory that is at least as aligned as src.
  Align srcAlign = srcAlloca->getAlign();
  bool isDestSufficientlyAligned = srcAlign <= cpyDestAlign;
  // If dest is not aligned enough and we can't increase its alignment then
  // bail out.
  if (!isDestSufficientlyAligned && !isa<AllocaInst>(cpyDest)) {
    LLVM_DEBUG(dbgs() << "Call Slot: Dest not sufficiently aligned\n");
    return false;
  }

  // Check that src is not accessed except via the call and the memcpy.  This
  // guarantees that it holds only undefined values when passed in (so the final
  // memcpy can be dropped), that it is not read or written between the call and
  // the memcpy, and that writing beyond the end of it is undefined.
  SmallVector<User *, 8> srcUseList(srcAlloca->users());
  while (!srcUseList.empty()) {
    User *U = srcUseList.pop_back_val();

    if (isa<BitCastInst>(U) || isa<AddrSpaceCastInst>(U)) {
      append_range(srcUseList, U->users());
      continue;
    }
    if (const auto *G = dyn_cast<GetElementPtrInst>(U)) {
      if (!G->hasAllZeroIndices())
        return false;

      append_range(srcUseList, U->users());
      continue;
    }
    if (const auto *IT = dyn_cast<IntrinsicInst>(U))
      if (IT->isLifetimeStartOrEnd())
        continue;

    if (U != C && U != cpyLoad)
      return false;
  }

  // Check whether src is captured by the called function, in which case there
  // may be further indirect uses of src.
  bool SrcIsCaptured = any_of(C->args(), [&](Use &U) {
    return U->stripPointerCasts() == cpySrc &&
           !C->doesNotCapture(C->getArgOperandNo(&U));
  });

  // If src is captured, then check whether there are any potential uses of
  // src through the captured pointer before the lifetime of src ends, either
  // due to a lifetime.end or a return from the function.
  if (SrcIsCaptured) {
    // Check that dest is not captured before/at the call. We have already
    // checked that src is not captured before it. If either had been captured,
    // then the call might be comparing the argument against the captured dest
    // or src pointer.
    Value *DestObj = getUnderlyingObject(cpyDest);
    if (!isIdentifiedFunctionLocal(DestObj) ||
        PointerMayBeCapturedBefore(DestObj, /* ReturnCaptures */ true,
                                   /* StoreCaptures */ true, C, DT,
                                   /* IncludeI */ true))
      return false;

    MemoryLocation SrcLoc =
        MemoryLocation(srcAlloca, LocationSize::precise(srcSize));
    for (Instruction &I :
         make_range(++C->getIterator(), C->getParent()->end())) {
      // Lifetime of srcAlloca ends at lifetime.end.
      if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
        if (II->getIntrinsicID() == Intrinsic::lifetime_end &&
            II->getArgOperand(1)->stripPointerCasts() == srcAlloca &&
            cast<ConstantInt>(II->getArgOperand(0))->uge(srcSize))
          break;
      }

      // Lifetime of srcAlloca ends at return.
      if (isa<ReturnInst>(&I))
        break;

      // Ignore the direct read of src in the load.
      if (&I == cpyLoad)
        continue;

      // Check whether this instruction may mod/ref src through the captured
      // pointer (we have already any direct mod/refs in the loop above).
      // Also bail if we hit a terminator, as we don't want to scan into other
      // blocks.
      if (isModOrRefSet(BAA.getModRefInfo(&I, SrcLoc)) || I.isTerminator())
        return false;
    }
  }

  // Since we're changing the parameter to the callsite, we need to make sure
  // that what would be the new parameter dominates the callsite.
  if (!DT->dominates(cpyDest, C)) {
    // Support moving a constant index GEP before the call.
    auto *GEP = dyn_cast<GetElementPtrInst>(cpyDest);
    if (GEP && GEP->hasAllConstantIndices() &&
        DT->dominates(GEP->getPointerOperand(), C))
      GEP->moveBefore(C);
    else
      return false;
  }

  // In addition to knowing that the call does not access src in some
  // unexpected manner, for example via a global, which we deduce from
  // the use analysis, we also need to know that it does not sneakily
  // access dest.  We rely on AA to figure this out for us.
  MemoryLocation DestWithSrcSize(cpyDest, LocationSize::precise(srcSize));
  ModRefInfo MR = BAA.getModRefInfo(C, DestWithSrcSize);
  // If necessary, perform additional analysis.
  if (isModOrRefSet(MR))
    MR = BAA.callCapturesBefore(C, DestWithSrcSize, DT);
  if (isModOrRefSet(MR))
    return false;

  // We can't create address space casts here because we don't know if they're
  // safe for the target.
  if (cpySrc->getType()->getPointerAddressSpace() !=
      cpyDest->getType()->getPointerAddressSpace())
    return false;
  for (unsigned ArgI = 0; ArgI < C->arg_size(); ++ArgI)
    if (C->getArgOperand(ArgI)->stripPointerCasts() == cpySrc &&
        cpySrc->getType()->getPointerAddressSpace() !=
            C->getArgOperand(ArgI)->getType()->getPointerAddressSpace())
      return false;

  // All the checks have passed, so do the transformation.
  bool changedArgument = false;
  for (unsigned ArgI = 0; ArgI < C->arg_size(); ++ArgI)
    if (C->getArgOperand(ArgI)->stripPointerCasts() == cpySrc) {
      Value *Dest = cpySrc->getType() == cpyDest->getType() ?  cpyDest
        : CastInst::CreatePointerCast(cpyDest, cpySrc->getType(),
                                      cpyDest->getName(), C);
      changedArgument = true;
      if (C->getArgOperand(ArgI)->getType() == Dest->getType())
        C->setArgOperand(ArgI, Dest);
      else
        C->setArgOperand(ArgI, CastInst::CreatePointerCast(
                                   Dest, C->getArgOperand(ArgI)->getType(),
                                   Dest->getName(), C));
    }

  if (!changedArgument)
    return false;

  // If the destination wasn't sufficiently aligned then increase its alignment.
  if (!isDestSufficientlyAligned) {
    assert(isa<AllocaInst>(cpyDest) && "Can only increase alloca alignment!");
    cast<AllocaInst>(cpyDest)->setAlignment(srcAlign);
  }

  if (SkippedLifetimeStart) {
    SkippedLifetimeStart->moveBefore(C);
    MSSAU->moveBefore(MSSA->getMemoryAccess(SkippedLifetimeStart),
                      MSSA->getMemoryAccess(C));
  }

  // Update AA metadata
  // FIXME: MD_tbaa_struct and MD_mem_parallel_loop_access should also be
  // handled here, but combineMetadata doesn't support them yet
  unsigned KnownIDs[] = {LLVMContext::MD_tbaa, LLVMContext::MD_alias_scope,
                         LLVMContext::MD_noalias,
                         LLVMContext::MD_invariant_group,
                         LLVMContext::MD_access_group};
  combineMetadata(C, cpyLoad, KnownIDs, true);
  if (cpyLoad != cpyStore)
    combineMetadata(C, cpyStore, KnownIDs, true);

  ++NumCallSlot;
  return true;
}

/// We've found that the (upward scanning) memory dependence of memcpy 'M' is
/// the memcpy 'MDep'. Try to simplify M to copy from MDep's input if we can.
bool MemCpyOptPass::processMemCpyMemCpyDependence(MemCpyInst *M,
                                                  MemCpyInst *MDep,
                                                  BatchAAResults &BAA) {
  // We can only transforms memcpy's where the dest of one is the source of the
  // other.
  if (M->getSource() != MDep->getDest() || MDep->isVolatile())
    return false;

  // If dep instruction is reading from our current input, then it is a noop
  // transfer and substituting the input won't change this instruction.  Just
  // ignore the input and let someone else zap MDep.  This handles cases like:
  //    memcpy(a <- a)
  //    memcpy(b <- a)
  if (M->getSource() == MDep->getSource())
    return false;

  // Second, the length of the memcpy's must be the same, or the preceding one
  // must be larger than the following one.
  if (MDep->getLength() != M->getLength()) {
    auto *MDepLen = dyn_cast<ConstantInt>(MDep->getLength());
    auto *MLen = dyn_cast<ConstantInt>(M->getLength());
    if (!MDepLen || !MLen || MDepLen->getZExtValue() < MLen->getZExtValue())
      return false;
  }

  // Verify that the copied-from memory doesn't change in between the two
  // transfers.  For example, in:
  //    memcpy(a <- b)
  //    *b = 42;
  //    memcpy(c <- a)
  // It would be invalid to transform the second memcpy into memcpy(c <- b).
  //
  // TODO: If the code between M and MDep is transparent to the destination "c",
  // then we could still perform the xform by moving M up to the first memcpy.
  // TODO: It would be sufficient to check the MDep source up to the memcpy
  // size of M, rather than MDep.
  if (writtenBetween(MSSA, BAA, MemoryLocation::getForSource(MDep),
                     MSSA->getMemoryAccess(MDep), MSSA->getMemoryAccess(M)))
    return false;

  // If the dest of the second might alias the source of the first, then the
  // source and dest might overlap. In addition, if the source of the first
  // points to constant memory, they won't overlap by definition. Otherwise, we
  // still want to eliminate the intermediate value, but we have to generate a
  // memmove instead of memcpy.
  bool UseMemMove = false;
  if (isModSet(BAA.getModRefInfo(M, MemoryLocation::getForSource(MDep))))
    UseMemMove = true;

  // If all checks passed, then we can transform M.
  LLVM_DEBUG(dbgs() << "MemCpyOptPass: Forwarding memcpy->memcpy src:\n"
                    << *MDep << '\n' << *M << '\n');

  // TODO: Is this worth it if we're creating a less aligned memcpy? For
  // example we could be moving from movaps -> movq on x86.
  IRBuilder<> Builder(M);
  Instruction *NewM;
  if (UseMemMove)
    NewM = Builder.CreateMemMove(M->getRawDest(), M->getDestAlign(),
                                 MDep->getRawSource(), MDep->getSourceAlign(),
                                 M->getLength(), M->isVolatile());
  else if (isa<MemCpyInlineInst>(M)) {
    // llvm.memcpy may be promoted to llvm.memcpy.inline, but the converse is
    // never allowed since that would allow the latter to be lowered as a call
    // to an external function.
    NewM = Builder.CreateMemCpyInline(
        M->getRawDest(), M->getDestAlign(), MDep->getRawSource(),
        MDep->getSourceAlign(), M->getLength(), M->isVolatile());
  } else
    NewM = Builder.CreateMemCpy(M->getRawDest(), M->getDestAlign(),
                                MDep->getRawSource(), MDep->getSourceAlign(),
                                M->getLength(), M->isVolatile());
  NewM->copyMetadata(*M, LLVMContext::MD_DIAssignID);

  assert(isa<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(M)));
  auto *LastDef = cast<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(M));
  auto *NewAccess = MSSAU->createMemoryAccessAfter(NewM, LastDef, LastDef);
  MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/true);

  // Remove the instruction we're replacing.
  eraseInstruction(M);
  ++NumMemCpyInstr;
  return true;
}

/// We've found that the (upward scanning) memory dependence of \p MemCpy is
/// \p MemSet.  Try to simplify \p MemSet to only set the trailing bytes that
/// weren't copied over by \p MemCpy.
///
/// In other words, transform:
/// \code
///   memset(dst, c, dst_size);
///   memcpy(dst, src, src_size);
/// \endcode
/// into:
/// \code
///   memcpy(dst, src, src_size);
///   memset(dst + src_size, c, dst_size <= src_size ? 0 : dst_size - src_size);
/// \endcode
bool MemCpyOptPass::processMemSetMemCpyDependence(MemCpyInst *MemCpy,
                                                  MemSetInst *MemSet,
                                                  BatchAAResults &BAA) {
  // We can only transform memset/memcpy with the same destination.
  if (!BAA.isMustAlias(MemSet->getDest(), MemCpy->getDest()))
    return false;

  // Check that src and dst of the memcpy aren't the same. While memcpy
  // operands cannot partially overlap, exact equality is allowed.
  if (isModSet(BAA.getModRefInfo(MemCpy, MemoryLocation::getForSource(MemCpy))))
    return false;

  // We know that dst up to src_size is not written. We now need to make sure
  // that dst up to dst_size is not accessed. (If we did not move the memset,
  // checking for reads would be sufficient.)
  if (accessedBetween(BAA, MemoryLocation::getForDest(MemSet),
                      MSSA->getMemoryAccess(MemSet),
                      MSSA->getMemoryAccess(MemCpy)))
    return false;

  // Use the same i8* dest as the memcpy, killing the memset dest if different.
  Value *Dest = MemCpy->getRawDest();
  Value *DestSize = MemSet->getLength();
  Value *SrcSize = MemCpy->getLength();

  if (mayBeVisibleThroughUnwinding(Dest, MemSet, MemCpy))
    return false;

  // If the sizes are the same, simply drop the memset instead of generating
  // a replacement with zero size.
  if (DestSize == SrcSize) {
    eraseInstruction(MemSet);
    return true;
  }

  // By default, create an unaligned memset.
  Align Alignment = Align(1);
  // If Dest is aligned, and SrcSize is constant, use the minimum alignment
  // of the sum.
  const Align DestAlign = std::max(MemSet->getDestAlign().valueOrOne(),
                                   MemCpy->getDestAlign().valueOrOne());
  if (DestAlign > 1)
    if (auto *SrcSizeC = dyn_cast<ConstantInt>(SrcSize))
      Alignment = commonAlignment(DestAlign, SrcSizeC->getZExtValue());

  IRBuilder<> Builder(MemCpy);

  // If the sizes have different types, zext the smaller one.
  if (DestSize->getType() != SrcSize->getType()) {
    if (DestSize->getType()->getIntegerBitWidth() >
        SrcSize->getType()->getIntegerBitWidth())
      SrcSize = Builder.CreateZExt(SrcSize, DestSize->getType());
    else
      DestSize = Builder.CreateZExt(DestSize, SrcSize->getType());
  }

  Value *Ule = Builder.CreateICmpULE(DestSize, SrcSize);
  Value *SizeDiff = Builder.CreateSub(DestSize, SrcSize);
  Value *MemsetLen = Builder.CreateSelect(
      Ule, ConstantInt::getNullValue(DestSize->getType()), SizeDiff);
  unsigned DestAS = Dest->getType()->getPointerAddressSpace();
  Instruction *NewMemSet = Builder.CreateMemSet(
      Builder.CreateGEP(
          Builder.getInt8Ty(),
          Builder.CreatePointerCast(Dest, Builder.getInt8PtrTy(DestAS)),
          SrcSize),
      MemSet->getOperand(1), MemsetLen, Alignment);

  assert(isa<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(MemCpy)) &&
         "MemCpy must be a MemoryDef");
  // The new memset is inserted after the memcpy, but it is known that its
  // defining access is the memset about to be removed which immediately
  // precedes the memcpy.
  auto *LastDef =
      cast<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(MemCpy));
  auto *NewAccess = MSSAU->createMemoryAccessBefore(
      NewMemSet, LastDef->getDefiningAccess(), LastDef);
  MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/true);

  eraseInstruction(MemSet);
  return true;
}

/// Determine whether the instruction has undefined content for the given Size,
/// either because it was freshly alloca'd or started its lifetime.
static bool hasUndefContents(MemorySSA *MSSA, BatchAAResults &AA, Value *V,
                             MemoryDef *Def, Value *Size) {
  if (MSSA->isLiveOnEntryDef(Def))
    return isa<AllocaInst>(getUnderlyingObject(V));

  if (auto *II = dyn_cast_or_null<IntrinsicInst>(Def->getMemoryInst())) {
    if (II->getIntrinsicID() == Intrinsic::lifetime_start) {
      auto *LTSize = cast<ConstantInt>(II->getArgOperand(0));

      if (auto *CSize = dyn_cast<ConstantInt>(Size)) {
        if (AA.isMustAlias(V, II->getArgOperand(1)) &&
            LTSize->getZExtValue() >= CSize->getZExtValue())
          return true;
      }

      // If the lifetime.start covers a whole alloca (as it almost always
      // does) and we're querying a pointer based on that alloca, then we know
      // the memory is definitely undef, regardless of how exactly we alias.
      // The size also doesn't matter, as an out-of-bounds access would be UB.
      if (auto *Alloca = dyn_cast<AllocaInst>(getUnderlyingObject(V))) {
        if (getUnderlyingObject(II->getArgOperand(1)) == Alloca) {
          const DataLayout &DL = Alloca->getModule()->getDataLayout();
          if (std::optional<TypeSize> AllocaSize =
                  Alloca->getAllocationSizeInBits(DL))
            if (*AllocaSize == LTSize->getValue() * 8)
              return true;
        }
      }
    }
  }

  return false;
}

/// Transform memcpy to memset when its source was just memset.
/// In other words, turn:
/// \code
///   memset(dst1, c, dst1_size);
///   memcpy(dst2, dst1, dst2_size);
/// \endcode
/// into:
/// \code
///   memset(dst1, c, dst1_size);
///   memset(dst2, c, dst2_size);
/// \endcode
/// When dst2_size <= dst1_size.
bool MemCpyOptPass::performMemCpyToMemSetOptzn(MemCpyInst *MemCpy,
                                               MemSetInst *MemSet,
                                               BatchAAResults &BAA) {
  // Make sure that memcpy(..., memset(...), ...), that is we are memsetting and
  // memcpying from the same address. Otherwise it is hard to reason about.
  if (!BAA.isMustAlias(MemSet->getRawDest(), MemCpy->getRawSource()))
    return false;

  Value *MemSetSize = MemSet->getLength();
  Value *CopySize = MemCpy->getLength();

  if (MemSetSize != CopySize) {
    // Make sure the memcpy doesn't read any more than what the memset wrote.
    // Don't worry about sizes larger than i64.

    // A known memset size is required.
    auto *CMemSetSize = dyn_cast<ConstantInt>(MemSetSize);
    if (!CMemSetSize)
      return false;

    // A known memcpy size is also required.
    auto  *CCopySize = dyn_cast<ConstantInt>(CopySize);
    if (!CCopySize)
      return false;
    if (CCopySize->getZExtValue() > CMemSetSize->getZExtValue()) {
      // If the memcpy is larger than the memset, but the memory was undef prior
      // to the memset, we can just ignore the tail. Technically we're only
      // interested in the bytes from MemSetSize..CopySize here, but as we can't
      // easily represent this location, we use the full 0..CopySize range.
      MemoryLocation MemCpyLoc = MemoryLocation::getForSource(MemCpy);
      bool CanReduceSize = false;
      MemoryUseOrDef *MemSetAccess = MSSA->getMemoryAccess(MemSet);
      MemoryAccess *Clobber = MSSA->getWalker()->getClobberingMemoryAccess(
          MemSetAccess->getDefiningAccess(), MemCpyLoc, BAA);
      if (auto *MD = dyn_cast<MemoryDef>(Clobber))
        if (hasUndefContents(MSSA, BAA, MemCpy->getSource(), MD, CopySize))
          CanReduceSize = true;

      if (!CanReduceSize)
        return false;
      CopySize = MemSetSize;
    }
  }

  IRBuilder<> Builder(MemCpy);
  Instruction *NewM =
      Builder.CreateMemSet(MemCpy->getRawDest(), MemSet->getOperand(1),
                           CopySize, MaybeAlign(MemCpy->getDestAlignment()));
  auto *LastDef =
      cast<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(MemCpy));
  auto *NewAccess = MSSAU->createMemoryAccessAfter(NewM, LastDef, LastDef);
  MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/true);

  return true;
}

// These helper classes are used for the stack-move optimization. See the
// comments above performStackMoveOptzn() for more details.

namespace {

// Tracks liveness on the basic block level. This is conservative; see the
// comments above performStackMoveOptzn() for justification.
class BasicBlockLiveness {
  // The earliest definition or use we've seen, combined with the three bits
  // below.
  PointerIntPair<Instruction *, 3> Value;

  // Whether the alloca is live-in to the block (from predecessor basic blocks).
  using LiveIn = Bitfield::Element<bool, 0, 1>;
  // Whether the alloca is live-out from the block (to successor basic blocks).
  using LiveOut = Bitfield::Element<bool, 1, 1>;
  // Whether there's at least one use of the alloca in this basic block. This
  // flag is important for detecting liveness conflicts, since the other
  // information stored here isn't sufficient to determine that a use is present
  // if a definition precedes it.
  using HasUse = Bitfield::Element<bool, 2, 1>;

  // Records a new def or use instruction.
  void setDefUseInst(Instruction *I) {
    assert((!hasDefUseInst() || I->comesBefore(getDefUseInst())) &&
           "Tried to overwrite an earlier def or use with a later one!");
    Value.setPointer(I);
  }

  // Sets the flag which determines whether this block has a use.
  void setHasUse(bool On) {
    unsigned V = Value.getInt();
    Bitfield::set<HasUse>(V, On);
    Value.setInt(V);
  }

public:
  BasicBlockLiveness() : Value(nullptr) {}

  // Returns the earliest definition or use we've seen in this block.
  Instruction *getDefUseInst() const { return Value.getPointer(); }
  // Returns true if there's a definition or use of the memory in this block.
  bool hasDefUseInst() const { return Value.getPointer() != nullptr; }
  // Returns true if the memory is live-in to this block (i.e. live-out of a
  // predecessor).
  bool isLiveIn() const { return Bitfield::get<LiveIn>(Value.getInt()); }
  // Returns true if the memory is live-out of this block (i.e. live-in to a
  // successor).
  bool isLiveOut() const { return Bitfield::get<LiveOut>(Value.getInt()); }
  // Returns true if there is at least one use of the memory in this block.
  bool hasUse() const { return Bitfield::get<HasUse>(Value.getInt()); }
  // Returns true if this alloca is live anywhere in this block or has
  // at least one use in it. If this returns false, the alloca is
  // guaranteed to be completely dead within this basic block.
  bool isLiveAnywhereOrHasUses() const {
    return isLiveIn() || isLiveOut() || hasUse();
  }

  // Records a new definition or use of the alloca being tracked within this
  // basic block.
  void update(Instruction *I, bool IsDef) {
    if (!hasDefUseInst() || I->comesBefore(getDefUseInst())) {
      setDefUseInst(I);
      setLiveIn(!IsDef);
    }
    if (!IsDef)
      setHasUse(true);
  }

  // Adjusts the live-in flag for this block.
  void setLiveIn(bool On) {
    unsigned V = Value.getInt();
    Bitfield::set<LiveIn>(V, On);
    Value.setInt(V);
  }

  // Adjusts the live-out flag for this block.
  void setLiveOut(bool On) {
    unsigned V = Value.getInt();
    Bitfield::set<LiveOut>(V, On);
    Value.setInt(V);
  }
};

using BasicBlockLivenessMap = DenseMap<BasicBlock *, BasicBlockLiveness>;

// Tracks uses of an alloca for the purposes of the stack-move optimization.
//
// This class does three things: (1) it makes sure that the alloca is never
// captured; (2) it records defs and uses of the alloca in a map for the
// liveness analysis to use; (3) it finds the nearest dominator and
// postdominator of all uses of this alloca for the purpose of lifetime
// intrinsic "shrink wrapping" if the optimization goes through.
class StackMoveTracker : public CaptureTracker {
  // Data layout info.
  const DataLayout &DL;
  // Dominator tree info.
  DominatorTree &DT;
  // Postdominator tree info.
  PostDominatorTree &PDT;
  // The memcpy instruction.
  Instruction *Store;
  // The size of the underlying alloca, in bits.
  TypeSize AllocaSizeInBits;

public:
  // Keeps track of the lifetime intrinsics that we find. We'll need to remove
  // these if the optimization goes through.
  SmallVector<IntrinsicInst *, 4> LifetimeMarkers;
  // Keeps track of instructions that have !noalias metadata. We need to drop
  // that metadata if the optimization succeeds.
  std::vector<Instruction *> NoAliasInstrs;
  // Liveness information for this alloca, tracked on the basic block level.
  BasicBlockLivenessMap BBLiveness;
  // Liveness information for this alloca, tracked on the instruction level for
  // the single basic block containing the memcpy.
  DenseMap<Instruction *, bool> StoreBBDefUseMap;
  // The nearest basic block that dominates all uses of the alloca that we've
  // seen so far. This is only null if we haven't seen any uses yet.
  BasicBlock *Dom;
  // The nearest basic block that postdominates all uses of the alloca that
  // we've seen so far. This can be null if there's no such postdominator.
  BasicBlock *PostDom;
  // The user that caused us to bail out, if any.
  User *AbortingUser;
  // Whether we should bail out of the stack-move optimization.
  bool Abort;

  StackMoveTracker(Instruction *Store, AllocaInst *Alloca, DominatorTree &DT,
                   PostDominatorTree &PDT)
      : DL(Store->getModule()->getDataLayout()), DT(DT), PDT(PDT), Store(Store),
        AllocaSizeInBits(*Alloca->getAllocationSizeInBits(DL)), Dom(nullptr),
        PostDom(nullptr), AbortingUser(nullptr), Abort(false) {}

private:
  // Called whenever we see a use or a definition of the alloca. If IsDef is
  // true, this is a def; otherwise, it's a use.
  void recordUseOrDef(Instruction *I, bool IsDef) {
    BasicBlock *BB = I->getParent();
    BBLiveness[BB].update(I, IsDef);

    // For the basic block containing the store, track liveness on the
    // instruction level.
    if (BB == Store->getParent())
      StoreBBDefUseMap[I] = IsDef;

    // If the instruction has !noalias metadata, record it so that we can delete
    // the metadata if the optimization succeeds.
    if (I->hasMetadata(LLVMContext::MD_noalias))
      NoAliasInstrs.push_back(I);
  }

public:
  // If there are too many uses, just bail out to avoid spending excessive
  // compile time.
  void tooManyUses() override { Abort = true; }

  // If the pointer was captured, we can't usefully track it, so just bail out.
  bool captured(const Use *U) override {
    if (!Abort) {
      AbortingUser = U->getUser();
      Abort = true;
      return true;
    }

    return false;
  }

  // Classifies a use as either a true use or a definition, records that, and
  // updates the nearest common dominator and postdominator accordingly.
  bool visitUse(const Use *U) override {
    Instruction *I = cast<Instruction>(U->getUser());
    BasicBlock *BB = I->getParent();

    // GEPs don't count as uses of the alloca memory (just of the pointer to the
    // alloca), so we don't care about them here.
    if (isa<GetElementPtrInst>(I) && U->getOperandNo() == 0)
      return false;

    // Update the nearest common dominator and postdominator. We know that this
    // is the first use if Dom is null, because multiple blocks always have a
    // mutual common dominator (though not necessarily a common postdominator).
    if (Dom == nullptr) {
      Dom = PostDom = BB;
    } else {
      Dom = DT.findNearestCommonDominator(Dom, BB);
      if (PostDom != nullptr)
        PostDom = PDT.findNearestCommonDominator(PostDom, BB);
    }

    // If an instruction overwrites all bytes of the alloca, it's a definition,
    // not a use. Detect those cases here.
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(I)) {
      if (II->isLifetimeStartOrEnd()) {
        // We treat a call to a lifetime intrinsic that covers the entire alloca
        // as a definition, since both llvm.lifetime.start and llvm.lifetime.end
        // intrinsics conceptually fill all the bytes of the alloca with an
        // undefined value. We also note these locations of these intrinsic
        // calls so that we can delete them later if the optimization succeeds.
        int64_t Size = cast<ConstantInt>(II->getArgOperand(0))->getSExtValue();
        if (Size < 0 || uint64_t(Size) * 8 == AllocaSizeInBits) {
          recordUseOrDef(II, true);
          LifetimeMarkers.push_back(II);
          return false;
        }
      } else if (MemIntrinsic *MI = dyn_cast<MemIntrinsic>(II)) {
        if (MI->getArgOperandNo(U) == 0) {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(MI->getLength())) {
            if (CI->getZExtValue() * 8 == AllocaSizeInBits.getFixedSize()) {
              // Memcpy, memmove, and memset instructions that fill every byte
              // of the alloca are definitions.
              recordUseOrDef(MI, true);
              return false;
            }
          }
        }
      }
    } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
      // Stores that overwrite all bytes of the alloca are definitions.
      if (U->getOperandNo() == 1 &&
          DL.getTypeStoreSizeInBits(SI->getValueOperand()->getType()) ==
              AllocaSizeInBits.getFixedSize()) {
        recordUseOrDef(SI, true);
        return false;
      }
    }

    // Otherwise, this instruction is a use. Make a note of that fact and
    // continue.
    recordUseOrDef(I, false);
    return false;
  }
};

} // namespace

// Performs liveness dataflow analysis for an alloca at the basic block level as
// part of the stack-move optimization.
//
// This implements the "backwards variable-at-a-time" variant of liveness
// analysis, propagating liveness information backwards from uses until it sees
// a basic block with a definition or one in which the variable is already
// live-out. As implemented, this is a linear-time algorithm, because it only
// visits every basic block at most once and the number of tracked variables is
// constant (two--the source and destination of the memcpy).
//
// In order to avoid spending too much compile time, this operates on the level
// of basic blocks instead of instructions, making it a conservative
// analysis. See the comments in performStackMoveOptzn() for more details.
//
// Returns true if the analysis succeeded or false if it failed due to examining
// too many basic blocks.
static bool computeLiveness(BasicBlockLivenessMap &BBLiveness) {
  // Start by initializing a worklist with all basic blocks that are live-in
  // (i.e. they potentially need to propagate liveness to their predecessors).
  SmallVector<BasicBlock *, 8> Worklist;
  for (auto &Pair : BBLiveness) {
    if (Pair.second.isLiveIn())
      Worklist.push_back(Pair.first);
  }

  // Iterate until we have no more blocks to process.
  unsigned Count = 0;
  while (!Worklist.empty()) {
    BasicBlock *BB = Worklist.back();
    Worklist.pop_back();

    // Cap the number of basic blocks we examine in order to avoid blowing up
    // compile time. The default threshold was empirically determined to be
    // sufficient 90% of the time in the Rust compiler.
    ++Count;
    if (Count >= MemCpyOptStackMoveThreshold) {
      LLVM_DEBUG(
          dbgs()
          << "Stack Move: Exceeded max basic block threshold, bailing\n");
      return false;
    }

    // We know that the alloca must be live-in to this basic block, or else we
    // wouldn't have added the block to the worklist in the first place.
    assert(BBLiveness.lookup(BB).isLiveIn() &&
           "Shouldn't have added a BB that wasn't live-in to the worklist!");

    // Propagate liveness back to predecessors.
    for (BasicBlock *Pred : predecessors(BB)) {
      BasicBlockLiveness PredLiveness = BBLiveness.lookup(Pred);

      // Skip predecessors in which the variable is already known to be
      // live-out.
      if (!PredLiveness.isLiveOut()) {
        PredLiveness.setLiveOut(true);

        // Don't enqueue predecessors if they contain direct defs or uses of the
        // variable. If a predecessor contains a use of the variable that
        // dominates all the other uses or defs of the variable within that
        // block, then we already added that predecessor to the worklist at the
        // beginning of this procedure, so we don't need to add it again. If, on
        // the other hand, the predecessor contains a definition of the variable
        // that dominates all the other uses or defs of the variable within the
        // block, then the predecessor won't propagate any liveness to *its*
        // predecessors, so we don't need to enqueue it either.
        if (!PredLiveness.hasDefUseInst()) {
          // We know that this predecessor is a basic block that contains
          // neither defs nor uses of the variable and in which the variable is
          // live-out. So the variable must be live-in to this predecessor too.
          PredLiveness.setLiveIn(true);
          Worklist.push_back(Pred);
        }

        BBLiveness[Pred] = PredLiveness;
      }
    }
  }

  return true;
}

// Returns true if the alloca is at the start of the entry block, modulo a few
// instructions like GEPs and debug info. We only perform the stack-move
// optimization for such allocas, which simplifies the logic.
static bool allocaIsAtStartOfEntryBlock(AllocaInst *AI) {
  BasicBlock *BB = AI->getParent();
  if (!BB->isEntryBlock()) {
    LLVM_DEBUG(dbgs() << "Stack Move: Alloca isn't in entry block\n");
    return false;
  }

  for (Instruction &I : *BB) {
    if (&I == AI)
      return true;
    if (isa<AllocaInst>(I) || isa<GetElementPtrInst>(I) ||
        isa<DbgInfoIntrinsic>(I) || I.isLifetimeStartOrEnd()) {
      continue;
    }
    LLVM_DEBUG(
        dbgs()
        << "Stack Move: Alloca isn't at start of entry block\n  Instruction:"
        << I << "\n");
    return false;
  }

  llvm_unreachable("Alloca wasn't found in its parent basic block");
}

// Attempts to optimize the pattern whereby memory is copied from an alloca to
// another alloca, where the two allocas aren't live simultaneously except
// during the transfer. If successful, the two allocas can be merged into one
// and the transfer can be deleted. This pattern is generated frequently in
// Rust, due to the ubiquity of move operations in that language.
//
// We choose to limit this optimization to cases in which neither alloca was
// captured, in order to avoid interprocedural analysis. As it turns out, the
// same CaptureTracking framework that is needed to detect this condition also
// turns out to be useful for gathering definitions and uses. So our general
// approach is to run CaptureTracking to find captures and simultaneously gather
// up uses and defs, followed by the standard liveness dataflow analysis to
// ensure that the source and destination aren't simultaneously live anywhere.
//
// To avoid blowing up compile time, we perform the liveness analysis
// conservatively on the basic block level rather than on the instruction level,
// with the exception of the basic block containing the memcpy itself. This
// means that any basic block that contains a use of both the source and
// destination causes us to conservatively bail out, even if the source and
// destination aren't actually simultaneously live. Empirically, this happens
// less than 2% of the time in typical Rust code, making the
// precision/compile-time tradeoff well worth it.
//
// Once we determine that the optimization is safe to perform, we replace all
// uses of the destination alloca with the source alloca. We also "shrink wrap"
// the lifetime markers of the single merged alloca to the nearest dominating
// and postdominating basic block. Note that the "shrink wrapping" procedure is
// a safe transformation only because we restrict the scope of this optimization
// to allocas that aren't captured.
bool MemCpyOptPass::performStackMoveOptzn(Instruction *Load, Instruction *Store,
                                          AllocaInst *DestAlloca,
                                          AllocaInst *SrcAlloca,
                                          uint64_t Size) {
  // If the optimization is disabled, forget it.
  if (MemCpyOptStackMoveThreshold == 0)
    return false;

  LLVM_DEBUG(dbgs() << "Stack Move: Attempting to optimize:\n"
                    << *Store << "\n");

  // Make sure the two allocas are in the same address space.
  if (SrcAlloca->getAddressSpace() != DestAlloca->getAddressSpace()) {
    LLVM_DEBUG(dbgs() << "Stack Move: Address space mismatch\n");
    return false;
  }

  // Calculate the static size of the allocas to be merged, bailing out if we
  // can't.
  const DataLayout &DL = DestAlloca->getModule()->getDataLayout();
  std::optional<TypeSize> SrcSize = SrcAlloca->getAllocationSizeInBits(DL);
  if (!SrcSize || SrcSize->isScalable() ||
      Size * 8 != SrcSize->getFixedSize()) {
    LLVM_DEBUG(dbgs() << "Stack Move: Source alloca size mismatch\n");
    return false;
  }
  std::optional<TypeSize> DestSize = DestAlloca->getAllocationSizeInBits(DL);
  if (!DestSize || DestSize->isScalable() ||
      Size * 8 != DestSize->getFixedSize()) {
    LLVM_DEBUG(dbgs() << "Stack Move: Destination alloca size mismatch\n");
    return false;
  }

  // Make sure the allocas are at the start of the entry block. This lets us
  // avoid having to do annoying checks to ensure the allocas dominate their
  // uses, as well as problems related to llvm.stacksave and llvm.stackrestore
  // intrinsics.
  if (!allocaIsAtStartOfEntryBlock(DestAlloca) ||
      !allocaIsAtStartOfEntryBlock(SrcAlloca)) {
    return false;
  }

  // Gather up all uses of the destination. Make sure that it wasn't captured
  // anywhere.
  StackMoveTracker DestTracker(Store, DestAlloca, *DT, *PDT);
  PointerMayBeCaptured(DestAlloca, &DestTracker);
  if (DestTracker.Abort) {
    LLVM_DEBUG({
      dbgs() << "Stack Move: Destination was captured:";
      if (DestTracker.AbortingUser != nullptr)
        dbgs() << "\n" << *DestTracker.AbortingUser;
      dbgs() << "\n";
    });
    return false;
  }

  // Likewise, collect all uses of the source, again making sure that it wasn't
  // captured anywhere.
  StackMoveTracker SrcTracker(Store, SrcAlloca, *DT, *PDT);
  PointerMayBeCaptured(SrcAlloca, &SrcTracker);
  if (SrcTracker.Abort) {
    LLVM_DEBUG({
      dbgs() << "Stack Move: Source was captured:";
      if (SrcTracker.AbortingUser != nullptr)
        dbgs() << "\n" << *SrcTracker.AbortingUser;
      dbgs() << "\n";
    });
    return false;
  }

  // Compute liveness on the basic block level.
  BasicBlock *StoreBB = Store->getParent();
  if (!computeLiveness(DestTracker.BBLiveness) ||
      !computeLiveness(SrcTracker.BBLiveness)) {
    return false;
  }

  // Check for liveness conflicts on the basic block level (with the exception
  // of the basic block containing the memcpy). This is conservative compared to
  // computing liveness on the instruction level. The precision loss is only 2%
  // on the Rust compiler, however, making this compile-time tradeoff
  // worthwhile.
  for (auto DestPair : DestTracker.BBLiveness) {
    BasicBlock *BB = DestPair.first;
    if (BB != StoreBB && DestPair.second.isLiveAnywhereOrHasUses() &&
        SrcTracker.BBLiveness.lookup(BB).isLiveAnywhereOrHasUses()) {
      LLVM_DEBUG(dbgs() << "Stack Move: Detected liveness conflict, "
                           "bailing:\n  Basic Block: "
                        << BB->getNameOrAsOperand() << "\n");
      return false;
    }
  }

  // Check liveness inside the single basic block containing the load and
  // store.
  bool DestLive = DestTracker.BBLiveness.lookup(StoreBB).isLiveOut();
  bool SrcLive = SrcTracker.BBLiveness.lookup(StoreBB).isLiveOut();
  for (auto &BI : reverse(*StoreBB)) {
    if (DestLive && SrcLive && &BI != Load && &BI != Store) {
      LLVM_DEBUG(
          dbgs() << "Stack Move: Detected liveness conflict inside the basic "
                    "block containing the memcpy, bailing:\n  Instruction: "
                 << BI << "\n");
      return false;
    }

    auto DestDefUseIt = DestTracker.StoreBBDefUseMap.find(&BI);
    auto SrcDefUseIt = SrcTracker.StoreBBDefUseMap.find(&BI);
    if (DestDefUseIt != DestTracker.StoreBBDefUseMap.end())
      DestLive = !DestDefUseIt->second;
    if (SrcDefUseIt != SrcTracker.StoreBBDefUseMap.end())
      SrcLive = !SrcDefUseIt->second;
  }

  // We can do the transformation. First, align the allocas appropriately.
  SrcAlloca->setAlignment(
      std::max(SrcAlloca->getAlign(), DestAlloca->getAlign()));

  // Merge the two allocas.
  DestAlloca->replaceAllUsesWith(SrcAlloca);

  // Drop metadata on the source alloca.
  SrcAlloca->dropUnknownNonDebugMetadata();

  // Now "shrink wrap" the lifetimes. Begin by creating a new lifetime start
  // marker at the start of the nearest common dominator of all defs and uses of
  // the merged alloca.
  //
  // We could be more precise here and query AA to find the latest point in the
  // basic block at which to place the call to the intrinsic, but that doesn't
  // seem worth it at the moment.
  assert(DestTracker.Dom != nullptr && SrcTracker.Dom != nullptr &&
         "There must be a common dominator for all defs and uses of the source "
         "and destination");
  Type *IntPtrTy =
      Type::getIntNTy(SrcAlloca->getContext(), DL.getPointerSizeInBits());
  ConstantInt *CI = cast<ConstantInt>(ConstantInt::get(IntPtrTy, Size));
  BasicBlock *Dom =
      DT->findNearestCommonDominator(DestTracker.Dom, SrcTracker.Dom);
  BasicBlock::iterator InsertionPt = Dom->getFirstNonPHIOrDbgOrAlloca();
  if (Dom == SrcAlloca->getParent() && InsertionPt != Dom->end() &&
      InsertionPt->comesBefore(SrcAlloca)) {
    // Make sure that the alloca dominates the lifetime start intrinsic.
    // Usually, the call to getFirstNonPHIOrDbgOrAlloca() above ensures that,
    // but if the allocas aren't all at the start of the basic block we might
    // have to fix things up.
    InsertionPt = ++BasicBlock::iterator(SrcAlloca);
  }
  IRBuilder<>(Dom, InsertionPt).CreateLifetimeStart(SrcAlloca, CI);

  // Next, create a new lifetime end marker at the end of the nearest common
  // postdominator of all defs and uses of the merged alloca, if there is one.
  // If there's no such postdominator, just don't bother; we could create one at
  // each exit block, but that'd be essentially semantically meaningless.
  if (DestTracker.PostDom != nullptr && SrcTracker.PostDom != nullptr) {
    if (BasicBlock *PostDom = PDT->findNearestCommonDominator(
            DestTracker.PostDom, SrcTracker.PostDom)) {
      // Edge case: It's possible that the terminating instruction of the
      // postdominating basic block is itself an invoke instruction that uses
      // the alloca. Placing the lifetime end intrinsic before that call would
      // be incorrect. Detect this situation and choose the next postdominator
      // instead.
      MemoryLocation Loc = MemoryLocation::getBeforeOrAfter(SrcAlloca);
      if (isModOrRefSet(AA->getModRefInfo(PostDom->getTerminator(), Loc))) {
        auto PostDomNode = (*PDT)[PostDom]->getIDom();
        PostDom = PostDomNode != nullptr ? PostDomNode->getBlock() : nullptr;
      }

      // Add the lifetime end intrinsic.
      if (PostDom != nullptr) {
        IRBuilder<>(PostDom, BasicBlock::iterator(PostDom->getTerminator()))
            .CreateLifetimeEnd(SrcAlloca, CI);
      }
    }
  }

  // Remove all other lifetime markers.
  for (IntrinsicInst *II : DestTracker.LifetimeMarkers)
    eraseInstruction(II);
  for (IntrinsicInst *II : SrcTracker.LifetimeMarkers)
    eraseInstruction(II);

  // As this transformation can cause memory accesses that didn't previously
  // alias to begin to alias one another, we remove !noalias metadata from any
  // uses of either alloca. This is conservative, but more precision doesn't
  // seem worthwhile right now.
  for (Instruction *I : DestTracker.NoAliasInstrs)
    I->setMetadata(LLVMContext::MD_noalias, nullptr);
  for (Instruction *I : SrcTracker.NoAliasInstrs)
    I->setMetadata(LLVMContext::MD_noalias, nullptr);

  // We're done! We don't need to delete the memcpy because later passes will do
  // it.
  LLVM_DEBUG(dbgs() << "Stack Move: Performed stack-move optimization\n");
  ++NumStackMove;
  return true;
}

/// Perform simplification of memcpy's.  If we have memcpy A
/// which copies X to Y, and memcpy B which copies Y to Z, then we can rewrite
/// B to be a memcpy from X to Z (or potentially a memmove, depending on
/// circumstances). This allows later passes to remove the first memcpy
/// altogether.
bool MemCpyOptPass::processMemCpy(MemCpyInst *M, BasicBlock::iterator &BBI) {
  // We can only optimize non-volatile memcpy's.
  if (M->isVolatile()) return false;

  // If the source and destination of the memcpy are the same, then zap it.
  if (M->getSource() == M->getDest()) {
    ++BBI;
    eraseInstruction(M);
    return true;
  }

  // If copying from a constant, try to turn the memcpy into a memset.
  if (auto *GV = dyn_cast<GlobalVariable>(M->getSource()))
    if (GV->isConstant() && GV->hasDefinitiveInitializer())
      if (Value *ByteVal = isBytewiseValue(GV->getInitializer(),
                                           M->getModule()->getDataLayout())) {
        IRBuilder<> Builder(M);
        Instruction *NewM =
            Builder.CreateMemSet(M->getRawDest(), ByteVal, M->getLength(),
                                 MaybeAlign(M->getDestAlignment()), false);
        auto *LastDef =
            cast<MemoryDef>(MSSAU->getMemorySSA()->getMemoryAccess(M));
        auto *NewAccess =
            MSSAU->createMemoryAccessAfter(NewM, LastDef, LastDef);
        MSSAU->insertDef(cast<MemoryDef>(NewAccess), /*RenameUses=*/true);

        eraseInstruction(M);
        ++NumCpyToSet;
        return true;
      }

  BatchAAResults BAA(*AA);
  MemoryUseOrDef *MA = MSSA->getMemoryAccess(M);
  // FIXME: Not using getClobberingMemoryAccess() here due to PR54682.
  MemoryAccess *AnyClobber = MA->getDefiningAccess();
  MemoryLocation DestLoc = MemoryLocation::getForDest(M);
  const MemoryAccess *DestClobber =
      MSSA->getWalker()->getClobberingMemoryAccess(AnyClobber, DestLoc, BAA);

  // Try to turn a partially redundant memset + memcpy into
  // memcpy + smaller memset.  We don't need the memcpy size for this.
  // The memcpy most post-dom the memset, so limit this to the same basic
  // block. A non-local generalization is likely not worthwhile.
  if (auto *MD = dyn_cast<MemoryDef>(DestClobber))
    if (auto *MDep = dyn_cast_or_null<MemSetInst>(MD->getMemoryInst()))
      if (DestClobber->getBlock() == M->getParent())
        if (processMemSetMemCpyDependence(M, MDep, BAA))
          return true;

  MemoryAccess *SrcClobber = MSSA->getWalker()->getClobberingMemoryAccess(
      AnyClobber, MemoryLocation::getForSource(M), BAA);

  // There are five possible optimizations we can do for memcpy:
  //   a) memcpy-memcpy xform which exposes redundance for DSE.
  //   b) call-memcpy xform for return slot optimization.
  //   c) memcpy from freshly alloca'd space or space that has just started
  //      its lifetime copies undefined data, and we can therefore eliminate
  //      the memcpy in favor of the data that was already at the destination.
  //   d) memcpy from a just-memset'd source can be turned into memset.
  //   e) elimination of memcpy via stack-move optimization.
  if (auto *MD = dyn_cast<MemoryDef>(SrcClobber)) {
    if (Instruction *MI = MD->getMemoryInst()) {
      if (auto *CopySize = dyn_cast<ConstantInt>(M->getLength())) {
        if (auto *C = dyn_cast<CallInst>(MI)) {
          if (performCallSlotOptzn(M, M, M->getDest(), M->getSource(),
                                   TypeSize::getFixed(CopySize->getZExtValue()),
                                   M->getDestAlign().valueOrOne(), BAA,
                                   [C]() -> CallInst * { return C; })) {
            LLVM_DEBUG(dbgs() << "Performed call slot optimization:\n"
                              << "    call: " << *C << "\n"
                              << "    memcpy: " << *M << "\n");
            eraseInstruction(M);
            ++NumMemCpyInstr;
            return true;
          }
        }
      }
      if (auto *MDep = dyn_cast<MemCpyInst>(MI)) {
        if (processMemCpyMemCpyDependence(M, MDep, BAA))
          return true;
      }
      if (auto *MDep = dyn_cast<MemSetInst>(MI)) {
        if (performMemCpyToMemSetOptzn(M, MDep, BAA)) {
          LLVM_DEBUG(dbgs() << "Converted memcpy to memset\n");
          eraseInstruction(M);
          ++NumCpyToSet;
          return true;
        }
      }
    }

    if (hasUndefContents(MSSA, BAA, M->getSource(), MD, M->getLength())) {
      LLVM_DEBUG(dbgs() << "Removed memcpy from undef\n");
      eraseInstruction(M);
      ++NumMemCpyInstr;
      return true;
    }
  }

  // If the transfer is from a stack slot to a stack slot, then we may be able
  // to perform the stack-move optimization. See the comments in
  // performStackMoveOptzn() for more details.
  AllocaInst *DestAlloca = dyn_cast<AllocaInst>(M->getDest());
  if (DestAlloca == nullptr)
    return false;
  AllocaInst *SrcAlloca = dyn_cast<AllocaInst>(M->getSource());
  if (SrcAlloca == nullptr)
    return false;
  ConstantInt *Len = dyn_cast<ConstantInt>(M->getLength());
  if (Len == nullptr)
    return false;
  if (performStackMoveOptzn(M, M, DestAlloca, SrcAlloca, Len->getZExtValue())) {
    // Avoid invalidating the iterator.
    BBI = M->getNextNonDebugInstruction()->getIterator();
    eraseInstruction(M);
    ++NumMemCpyInstr;
    return true;
  }

  return false;
}

/// Transforms memmove calls to memcpy calls when the src/dst are guaranteed
/// not to alias.
bool MemCpyOptPass::processMemMove(MemMoveInst *M) {
  // See if the source could be modified by this memmove potentially.
  if (isModSet(AA->getModRefInfo(M, MemoryLocation::getForSource(M))))
    return false;

  LLVM_DEBUG(dbgs() << "MemCpyOptPass: Optimizing memmove -> memcpy: " << *M
                    << "\n");

  // If not, then we know we can transform this.
  Type *ArgTys[3] = { M->getRawDest()->getType(),
                      M->getRawSource()->getType(),
                      M->getLength()->getType() };
  M->setCalledFunction(Intrinsic::getDeclaration(M->getModule(),
                                                 Intrinsic::memcpy, ArgTys));

  // For MemorySSA nothing really changes (except that memcpy may imply stricter
  // aliasing guarantees).

  ++NumMoveToCpy;
  return true;
}

/// This is called on every byval argument in call sites.
bool MemCpyOptPass::processByValArgument(CallBase &CB, unsigned ArgNo) {
  const DataLayout &DL = CB.getCaller()->getParent()->getDataLayout();
  // Find out what feeds this byval argument.
  Value *ByValArg = CB.getArgOperand(ArgNo);
  Type *ByValTy = CB.getParamByValType(ArgNo);
  TypeSize ByValSize = DL.getTypeAllocSize(ByValTy);
  MemoryLocation Loc(ByValArg, LocationSize::precise(ByValSize));
  MemoryUseOrDef *CallAccess = MSSA->getMemoryAccess(&CB);
  if (!CallAccess)
    return false;
  MemCpyInst *MDep = nullptr;
  BatchAAResults BAA(*AA);
  MemoryAccess *Clobber = MSSA->getWalker()->getClobberingMemoryAccess(
      CallAccess->getDefiningAccess(), Loc, BAA);
  if (auto *MD = dyn_cast<MemoryDef>(Clobber))
    MDep = dyn_cast_or_null<MemCpyInst>(MD->getMemoryInst());

  // If the byval argument isn't fed by a memcpy, ignore it.  If it is fed by
  // a memcpy, see if we can byval from the source of the memcpy instead of the
  // result.
  if (!MDep || MDep->isVolatile() ||
      ByValArg->stripPointerCasts() != MDep->getDest())
    return false;

  // The length of the memcpy must be larger or equal to the size of the byval.
  auto *C1 = dyn_cast<ConstantInt>(MDep->getLength());
  if (!C1 || !TypeSize::isKnownGE(
                 TypeSize::getFixed(C1->getValue().getZExtValue()), ByValSize))
    return false;

  // Get the alignment of the byval.  If the call doesn't specify the alignment,
  // then it is some target specific value that we can't know.
  MaybeAlign ByValAlign = CB.getParamAlign(ArgNo);
  if (!ByValAlign) return false;

  // If it is greater than the memcpy, then we check to see if we can force the
  // source of the memcpy to the alignment we need.  If we fail, we bail out.
  MaybeAlign MemDepAlign = MDep->getSourceAlign();
  if ((!MemDepAlign || *MemDepAlign < *ByValAlign) &&
      getOrEnforceKnownAlignment(MDep->getSource(), ByValAlign, DL, &CB, AC,
                                 DT) < *ByValAlign)
    return false;

  // The address space of the memcpy source must match the byval argument
  if (MDep->getSource()->getType()->getPointerAddressSpace() !=
      ByValArg->getType()->getPointerAddressSpace())
    return false;

  // Verify that the copied-from memory doesn't change in between the memcpy and
  // the byval call.
  //    memcpy(a <- b)
  //    *b = 42;
  //    foo(*a)
  // It would be invalid to transform the second memcpy into foo(*b).
  if (writtenBetween(MSSA, BAA, MemoryLocation::getForSource(MDep),
                     MSSA->getMemoryAccess(MDep), MSSA->getMemoryAccess(&CB)))
    return false;

  Value *TmpCast = MDep->getSource();
  if (MDep->getSource()->getType() != ByValArg->getType()) {
    BitCastInst *TmpBitCast = new BitCastInst(MDep->getSource(), ByValArg->getType(),
                                              "tmpcast", &CB);
    // Set the tmpcast's DebugLoc to MDep's
    TmpBitCast->setDebugLoc(MDep->getDebugLoc());
    TmpCast = TmpBitCast;
  }

  LLVM_DEBUG(dbgs() << "MemCpyOptPass: Forwarding memcpy to byval:\n"
                    << "  " << *MDep << "\n"
                    << "  " << CB << "\n");

  // Otherwise we're good!  Update the byval argument.
  CB.setArgOperand(ArgNo, TmpCast);
  ++NumMemCpyInstr;
  return true;
}

/// Executes one iteration of MemCpyOptPass.
bool MemCpyOptPass::iterateOnFunction(Function &F) {
  bool MadeChange = false;

  // Walk all instruction in the function.
  for (BasicBlock &BB : F) {
    // Skip unreachable blocks. For example processStore assumes that an
    // instruction in a BB can't be dominated by a later instruction in the
    // same BB (which is a scenario that can happen for an unreachable BB that
    // has itself as a predecessor).
    if (!DT->isReachableFromEntry(&BB))
      continue;

    for (BasicBlock::iterator BI = BB.begin(), BE = BB.end(); BI != BE;) {
        // Avoid invalidating the iterator.
      Instruction *I = &*BI++;

      bool RepeatInstruction = false;

      if (auto *SI = dyn_cast<StoreInst>(I))
        MadeChange |= processStore(SI, BI);
      else if (auto *M = dyn_cast<MemSetInst>(I))
        RepeatInstruction = processMemSet(M, BI);
      else if (auto *M = dyn_cast<MemCpyInst>(I))
        RepeatInstruction = processMemCpy(M, BI);
      else if (auto *M = dyn_cast<MemMoveInst>(I))
        RepeatInstruction = processMemMove(M);
      else if (auto *CB = dyn_cast<CallBase>(I)) {
        for (unsigned i = 0, e = CB->arg_size(); i != e; ++i)
          if (CB->isByValArgument(i))
            MadeChange |= processByValArgument(*CB, i);
      }

      // Reprocess the instruction if desired.
      if (RepeatInstruction) {
        if (BI != BB.begin())
          --BI;
        MadeChange = true;
      }
    }
  }

  return MadeChange;
}

PreservedAnalyses MemCpyOptPass::run(Function &F, FunctionAnalysisManager &AM) {
  auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto *AA = &AM.getResult<AAManager>(F);
  auto *AC = &AM.getResult<AssumptionAnalysis>(F);
  auto *DT = &AM.getResult<DominatorTreeAnalysis>(F);
  auto *PDT = &AM.getResult<PostDominatorTreeAnalysis>(F);
  auto *MSSA = &AM.getResult<MemorySSAAnalysis>(F);

  bool MadeChange = runImpl(F, &TLI, AA, AC, DT, PDT, &MSSA->getMSSA());
  if (!MadeChange)
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  PA.preserve<MemorySSAAnalysis>();
  return PA;
}

bool MemCpyOptPass::runImpl(Function &F, TargetLibraryInfo *TLI_,
                            AliasAnalysis *AA_, AssumptionCache *AC_,
                            DominatorTree *DT_, PostDominatorTree *PDT_,
                            MemorySSA *MSSA_) {
  bool MadeChange = false;
  TLI = TLI_;
  AA = AA_;
  AC = AC_;
  DT = DT_;
  PDT = PDT_;
  MSSA = MSSA_;
  MemorySSAUpdater MSSAU_(MSSA_);
  MSSAU = &MSSAU_;

  while (true) {
    if (!iterateOnFunction(F))
      break;
    MadeChange = true;
  }

  if (VerifyMemorySSA)
    MSSA_->verifyMemorySSA();

  return MadeChange;
}

/// This is the main transformation entry point for a function.
bool MemCpyOptLegacyPass::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto *TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  auto *AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
  auto *AC = &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  auto *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  auto *PDT = &getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
  auto *MSSA = &getAnalysis<MemorySSAWrapperPass>().getMSSA();

  return Impl.runImpl(F, TLI, AA, AC, DT, PDT, MSSA);
}
