//===- PropagateAndConvertNoAlias.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass moves dependencies on llvm.noalias onto the ptr_provenance.
// It also introduces and propagates llvm.provenance.noalias and
// llvm.noalias.arg.guard intrinsics.
//
// It is best placed as early as possible, but after: SROA+EarlyCSE
//  - SROA: SROA converts llvm.noalias.copy.guard into llvm.noalias
//  - EarlyCSE helps in cleaning up some expressions, make our work here easier.
//
// And after inlining: inlining can also expose new llvm.noalias intrinsics and
// extra information about the dependencies.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/PropagateAndConvertNoAlias.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include <map>
#include <type_traits>

using namespace llvm;

#define DEBUG_TYPE "convert-noalias"

namespace {

class PropagateAndConvertNoAliasLegacyPass : public FunctionPass {
public:
  static char ID;
  explicit PropagateAndConvertNoAliasLegacyPass() : FunctionPass(ID), Impl() {
    initializePropagateAndConvertNoAliasLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override {
    return "Propagate and Convert Noalias intrinsics";
  }

private:
  PropagateAndConvertNoAliasPass Impl;
};
} // namespace

char PropagateAndConvertNoAliasLegacyPass::ID = 0;
INITIALIZE_PASS_BEGIN(PropagateAndConvertNoAliasLegacyPass, "convert-noalias",
                      "Propagate And Convert llvm.noalias intrinsics", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(PropagateAndConvertNoAliasLegacyPass, "convert-noalias",
                    "Propagate And Convert llvm.noalias intrinsics", false,
                    false)

void PropagateAndConvertNoAliasLegacyPass::getAnalysisUsage(
    AnalysisUsage &AU) const {
  AU.addPreserved<GlobalsAAWrapperPass>();
  // FIXME: not sure the CallGraphWrapperPass is needed. It ensures the same
  // pass order is kept as if the PropagateAndConvertNoAlias pass was not there.
  AU.addPreserved<CallGraphWrapperPass>();
  AU.addPreserved<DominatorTreeWrapperPass>();
  AU.addRequiredTransitive<DominatorTreeWrapperPass>();
}

bool PropagateAndConvertNoAliasLegacyPass::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  return Impl.runImpl(F, getAnalysis<DominatorTreeWrapperPass>().getDomTree());
}

namespace llvm {

bool PropagateAndConvertNoAliasPass::runImpl(Function &F, DominatorTree &DT) {
  return doit(F, DT);
}

FunctionPass *createPropagateAndConvertNoAliasPass() {
  return new PropagateAndConvertNoAliasLegacyPass();
}

PreservedAnalyses
PropagateAndConvertNoAliasPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = runImpl(F, AM.getResult<DominatorTreeAnalysis>(F));

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<GlobalsAA>();
  // FIXME: not sure this is valid:
  //?? PA.preserve<CallGraphWrapperPass>(); // See above

  return PA;
}

typedef SmallVector<Instruction *, 10> ProvenanceWorklist;
typedef SmallVector<Instruction *, 2> DepsVector;
typedef std::map<Instruction *, DepsVector> I2Deps;
typedef SmallPtrSet<Instruction *, 10> InstructionSet;
typedef SmallPtrSet<BasicBlock *, 4> BasicBlockSet;

// Analyse and propagate the instructions that need provenances:
// - InstructionsForProvenance: instructions that need a provenance
// representation
// - at entry: (A)
// -- llvm.noalias  -> llvm.provenance.noalias
// -- llvm.noalias.arg.guard a, prov_a -> prov_a
//
// - during propagation: (B)
// -- select a, b, c  -> select a, prov_b, prov_c
// -- PHI a, b,... -> PHI prov_a, prov_b, ...
//
// - Handled: Instructions that have been investigated. The Deps side refers to
// the provenance dependency. (C)
// -- a nullptr indicates that the normal dependency must be used for that
// operand
// -- an I indictates that the provenance representation of I must be used for
// that operand
//
// The algorithm:
// - We start from the llvm.noalias and llvm.noalias.arg.guard instructions
// - We go over their users, and check if they are special or not
// -- special users need a provenance representation and are annotated as such
// in 'Handled' (non-empty Dep)
// -- normal instructions are a passthrough, and are annotated with an empty Dep
// in 'Handled' (I->{})
// -- some instructions stop the recursion:
// --- ICmp
// --- first arg of select
// --- llvm.provenance.noalias, llvm.noalias
//
// After the analysis, 'Handled' contains an overview of all instructions that
// depend on (A)
// - those instructions that were seen, but ignored otherwise have no
// dependencies (I -> {} )
// - instructions that refer to one ore more provenances have explicit
// dependencies. (I -> { op0, op1, op2, ... })
// -- if opX == nullptr -> not a real ptr_provenance dependency
// -- if opX == someI :
// ---- if someI points to an instruction in Handled, it must be one of the
// instructions that have a provenance representation
// ---- otherwise, it points to a not-handle plain dependency (coming from a
// noalias.arg.guard)
static void propagateInstructionsForProvenance(
    ProvenanceWorklist &InstructionsForProvenance, I2Deps &Handled,
    ProvenanceWorklist &out_CreationList, InstructionSet &ProvenancePHIs,
    BasicBlockSet &DeadBasicBlocks) {
  auto updateMatchingOperands = [](Instruction *U, Instruction *I,
                                   DepsVector &Deps, Instruction *I4SC) {
    assert(U->getNumOperands() == Deps.size());
    auto it = Deps.begin();
    for (Value *UOp : U->operands()) {
      if (UOp == I) {
        assert(*it == nullptr || *it == I4SC);
        *it = I4SC;
      }
      ++it;
    }
  };

  while (!InstructionsForProvenance.empty()) {
    Instruction *I4SC = InstructionsForProvenance.pop_back_val();
    LLVM_DEBUG(llvm::dbgs()
               << "-- Propagating provenance instruction: " << *I4SC << "\n");
    if (DeadBasicBlocks.count(I4SC->getParent())) {
      LLVM_DEBUG(llvm::dbgs() << "--- Skipped - dead basic block\n");
      continue;
    }
    SmallVector<Instruction *, 10> WorkList = {I4SC};
    if (auto *CB = dyn_cast<CallBase>(I4SC)) {
      if (CB->getIntrinsicID() == Intrinsic::noalias_arg_guard) {
        // llvm.noalias.arg.guard: delegate to ptr_provenance (operand 1)
        Handled.insert(I2Deps::value_type(I4SC, {}));
        // no need to add to out_CreationList

        assert(!isa<UndefValue>(I4SC->getOperand(0)) &&
               !isa<UndefValue>(I4SC->getOperand(1)) &&
               "Degenerated case must have been resolved already");
        assert(I4SC->getOperand(0) != I4SC->getOperand(1) &&
               "Degenerated case must have been resolved already");

        I4SC = dyn_cast<Instruction>(I4SC->getOperand(1));
        if (I4SC == nullptr) {
          // Provenance became a constant ? Then the arg guard is not needed
          // any more and there is nothing to propagate
          continue;
        }
      }
    }
    while (!WorkList.empty()) {
      Instruction *I = WorkList.pop_back_val();
      LLVM_DEBUG(llvm::dbgs() << "-- checking:" << *I << "\n");
      if (DeadBasicBlocks.count(I->getParent())) {
        LLVM_DEBUG(llvm::dbgs() << "--- skipped - dead basic block\n");
        continue;
      }
      bool isPtrToInt = isa<PtrToIntInst>(I);
      for (auto &UOp : I->uses()) {
        auto *U_ = UOp.getUser();
        LLVM_DEBUG(llvm::dbgs() << "--- used by:" << *U_
                                << ", operand:" << UOp.getOperandNo() << "\n");
        Instruction *U = dyn_cast<Instruction>(U_);
        if (U == nullptr)
          continue;

        // Only see through a ptr2int if it used by a int2ptr
        if (isPtrToInt && !isa<IntToPtrInst>(U))
          continue;

        if (isa<SelectInst>(U)) {
          // ======================================== select -> { lhs, rhs }
          bool MatchesOp1 = (U->getOperand(1) == I);
          bool MatchesOp2 = (U->getOperand(2) == I);

          if (MatchesOp1 || MatchesOp2) {
            auto HI = Handled.insert(I2Deps::value_type(U, {nullptr, nullptr}));
            if (HI.second)
              out_CreationList.push_back(U);
            if (MatchesOp1) {
              HI.first->second[0] = I4SC;
            }
            if (MatchesOp2) {
              HI.first->second[1] = I4SC;
            }
            if (HI.second) {
              InstructionsForProvenance.push_back(U);
            }
          }
        } else if (isa<LoadInst>(U)) {
          // ======================================== load -> { ptr }
          if (UOp.getOperandNo() ==
              LoadInst::getNoaliasProvenanceOperandIndex())
            continue; // tracking on provenance -> ignore

          auto HI = Handled.insert(I2Deps::value_type(U, {I4SC}));
          if (HI.second)
            out_CreationList.push_back(U);
          assert(U->getOperand(0) == I);
          if (HI.second) {
            // continue
          }
        } else if (isa<StoreInst>(U)) {
          // ======================================== store -> { val, ptr }
          if (UOp.getOperandNo() ==
              StoreInst::getNoaliasProvenanceOperandIndex())
            continue; // tracking on provenance -> ignore

          // also track if we are storing a restrict annotated pointer value...
          // This might provide useful information about 'escaping pointers'
          bool MatchesOp0 = (U->getOperand(0) == I);
          bool MatchesOp1 = (U->getOperand(1) == I);

          if (MatchesOp0 || MatchesOp1) {
            auto HI = Handled.insert(I2Deps::value_type(U, {nullptr, nullptr}));
            if (HI.second)
              out_CreationList.push_back(U);
            if (MatchesOp0) {
              HI.first->second[0] = I4SC;
            }
            if (MatchesOp1) {
              HI.first->second[1] = I4SC;
            }
          }
        } else if (isa<InsertValueInst>(U)) {
          // ======================================== insertvalue -> { val }
          // track for injecting llvm.noalias.arg.guard
          assert(U->getOperand(1) == I);
          // need to introduce a guard
          auto HI = Handled.insert(I2Deps::value_type(U, {I4SC}));
          if (HI.second)
            out_CreationList.push_back(U);
        } else if (isa<PtrToIntInst>(U)) {
          // ======================================== ptr2int -> { val }
          // track for injecting llvm.noalias.arg.guard
          assert(U->getOperand(0) == I);
          // need to introduce a guard
          auto HI = Handled.insert(I2Deps::value_type(U, {I4SC}));
          if (HI.second)
            out_CreationList.push_back(U);
        } else if (isa<ReturnInst>(U)) {
          auto HI = Handled.insert(I2Deps::value_type(U, {I4SC}));
          if (HI.second)
            out_CreationList.push_back(U);
        } else if (isa<PHINode>(U)) {
          // ======================================== PHI -> { ..... }
          PHINode *PU = cast<PHINode>(U);
          auto HI = Handled.insert(I2Deps::value_type(U, {}));
          if (HI.second) {
            HI.first->second.resize(U->getNumOperands(), nullptr);
            if (ProvenancePHIs.count(U) == 0) {
              // This is a normal PHI, consider it for propagation
              InstructionsForProvenance.push_back(U);
            }
            if (U->getNumOperands())
              out_CreationList.push_back(U);
          }
          updateMatchingOperands(PU, I, HI.first->second, I4SC);
        } else if (auto *CS = dyn_cast<CallBase>(U)) {
          // =============================== call/invoke/intrinsic -> { ...... }

          // NOTES:
          // - we always block at a call...
          // - the known intrinsics should not have any extra annotations
          switch (CS->getIntrinsicID()) {
          case Intrinsic::provenance_noalias:
          case Intrinsic::noalias: {
            bool MatchesOp0 = (U->getOperand(0) == I);
            bool MatchesOpP =
                (U->getOperand(Intrinsic::NoAliasIdentifyPArg) == I);
            static_assert(Intrinsic::NoAliasIdentifyPArg ==
                              Intrinsic::ProvenanceNoAliasIdentifyPArg,
                          "those must be identical");

            if (MatchesOp0 || MatchesOpP) {
              auto HI =
                  Handled.insert(I2Deps::value_type(U, {nullptr, nullptr}));
              if (HI.second)
                out_CreationList.push_back(U);
              if (MatchesOp0) {
                HI.first->second[0] = I4SC;
              }
              if (MatchesOpP) {
                HI.first->second[1] = I4SC;
              }
            }
            continue;
          }
          case Intrinsic::noalias_arg_guard: {
            // ignore - should be handled by the outer loop !
            continue;
          }

          default:
            break;
          }
          // if we get here, we need to inject guards for certain arguments.
          // Track which arguments will need one.
          auto HI = Handled.insert(I2Deps::value_type(U, {}));
          if (HI.second) {
            HI.first->second.resize(U->getNumOperands(), nullptr);
            if (U->getNumOperands()) {
              out_CreationList.push_back(U);
            }
          }
          updateMatchingOperands(U, I, HI.first->second, I4SC);
          if (I == CS->getReturnedArgOperand()) {
            // also see through call - this does not omit the need of
            // introducing a noalias_arg_guard
            WorkList.push_back(U);
          }
        } else {
          // ======================================== other -> {}
          // this is the generic case... not sure if we should have a elaborate
          // check for 'all other instructions'. just acknowledge that we saw it
          // and propagate to any users
          // - NOTE: if we happen have already handled it, this might indicate
          // something interesting that we should handle separately

          switch (U->getOpcode()) {
          case Instruction::ICmp:
            // restrict pointer used in comparison - do not propagate
            // provenance
            continue;
          default:
            break;
          }

          auto HI = Handled.insert(I2Deps::value_type(U, {}));
          // No need to add to out_CreationList
          if (!HI.second) {
            llvm::errs()
                << "WARNING: found an instruction that was already handled:"
                << *U << "\n";
            assert(!HI.second &&
                   "We should not encounter a handled instruction ??");
          }

          if (HI.second) {
            WorkList.push_back(U);
          }
        }
      }
    }
  }
}

typedef SmallDenseMap<std::pair<Value *, Type *>, Value *, 16>
    ValueType2CastMap;
static Value *createBitOrPointerOrAddrSpaceCast(Value *V, Type *T,
                                                ValueType2CastMap &VT2C) {
  if (V->getType() == T)
    return V;

  // Make sure we remember what casts we introduced
  Value *&Entry = VT2C[std::make_pair(V, T)];
  if (Entry == nullptr) {
    Instruction *InsertionPoint = cast<Instruction>(V);
    if (auto *PHI = dyn_cast<PHINode>(V)) {
      InsertionPoint = PHI->getParent()->getFirstNonPHI();
    } else {
      InsertionPoint = InsertionPoint->getNextNode();
    }

    IRBuilder<> Builder(InsertionPoint);
    Entry = Builder.CreateBitOrPointerCast(V, T);
  }
  return Entry;
}

static bool isValidProvenanceNoAliasInsertionPlace(IntrinsicInst *SNA,
                                                   Value *InsertionPointV,
                                                   DominatorTree &DT) {
  assert(SNA->getIntrinsicID() == Intrinsic::provenance_noalias &&
         "Expect a provenance.noalias");
  Instruction *InsertionPointI = dyn_cast<Instruction>(InsertionPointV);
  if (InsertionPointI == nullptr)
    return false;

  auto isDominatingOn = [&](Value *Arg) {
    auto *ArgI = dyn_cast<Instruction>(Arg);
    if (ArgI == nullptr)
      return true;
    return DT.dominates(ArgI, InsertionPointI);
  };

  for (auto Op : {Intrinsic::ProvenanceNoAliasNoAliasDeclArg,
                  Intrinsic::ProvenanceNoAliasIdentifyPArg,
                  Intrinsic::ProvenanceNoAliasIdentifyPProvenanceArg,
                  Intrinsic::ProvenanceNoAliasIdentifyPObjIdArg,
                  Intrinsic::ProvenanceNoAliasScopeArg}) {
    if (!isDominatingOn(SNA->getOperand(Op)))
      return false;
  }

  return true;
}

// combine llvm.provenance.noalias intrinsics as much as possible
void collapseProvenanceNoAlias(
    ProvenanceWorklist &CollapseableProvenanceNoAliasIntrinsics,
    DominatorTree &DT) {
  if (CollapseableProvenanceNoAliasIntrinsics.empty())
    return;

  if (!CollapseableProvenanceNoAliasIntrinsics.empty()) {
    // sweep from back to front, then from front to back etc... until no
    // modifications are done
    do {
      LLVM_DEBUG(llvm::dbgs()
                 << "- Trying to collapse llvm.provenance.noalias\n");
      ProvenanceWorklist NextList;
      bool Changed = false;

      // 1)  provenance.noaliasA (provenance.noaliasB (....), ...)  ->
      // provenance.noaliasB(...)
      while (!CollapseableProvenanceNoAliasIntrinsics.empty()) {
        IntrinsicInst *I =
            cast<IntrinsicInst>(CollapseableProvenanceNoAliasIntrinsics.back());
        assert(I->getIntrinsicID() == Intrinsic::provenance_noalias);

        CollapseableProvenanceNoAliasIntrinsics.pop_back();

        // provenance.noalias (provenance.noalias(....), .... )  ->
        // provenance.noalias(....)
        if (IntrinsicInst *DepI = dyn_cast<IntrinsicInst>(I->getOperand(0))) {
          // Check if the depending intrinsic is compatible)
          if (DepI->getIntrinsicID() == Intrinsic::provenance_noalias &&
              areProvenanceNoAliasCompatible(DepI, I)) {
            // similar enough - look through
            LLVM_DEBUG(llvm::dbgs() << "-- Collapsing(1):" << *I << "\n");
            I->replaceAllUsesWith(DepI);
            I->eraseFromParent();
            Changed = true;
            continue;
          }
        }

        if (PHINode *DepI = dyn_cast<PHINode>(I->getOperand(0))) {
          //@ FIXME: TODO: make more general ?
          // provenance.noalias(PHI (fum, self)) -> PHI(provenance.noalias(fum),
          // phi self ref)
          // - NOTE: only handle the 'simple' case for now ! At least that will
          // be correct.
          if ((DepI->getNumIncomingValues() == 2) &&
              (DepI->getNumUses() == 1)) {
            LLVM_DEBUG(llvm::dbgs()
                       << "--- Investigating interesting PHI depenceny\n");
            bool SelfDep0 = (DepI->getOperand(0) == I);
            bool SelfDep1 = (DepI->getOperand(1) == I);
            if (SelfDep0 || SelfDep1) {
              LLVM_DEBUG(llvm::dbgs() << "---- has self dependency\n");
              unsigned ChannelToFollow = SelfDep0 ? 1 : 0;
              // Try to find a possible insertion point
              if (isValidProvenanceNoAliasInsertionPlace(
                      I, DepI->getOperand(ChannelToFollow), DT)) {
                // create a new provenance.noalias at the insertion point
                // FIXME: if DepDepI is not an instruction, we could take the
                // end of the BB as insertion location ??
                LLVM_DEBUG(llvm::dbgs() << "----- Migrating !\n");
                Instruction *DepDepI =
                    cast<Instruction>(DepI->getOperand(ChannelToFollow));
                auto DepDepIIt = DepDepI->getIterator();
                if (isa<PHINode>(DepDepI)) {
                  DepDepIIt = DepDepI->getParent()->getFirstInsertionPt();
                } else {
                  ++DepDepIIt;
                }
                IRBuilder<> builder(DepDepI->getParent(), DepDepIIt);

                auto *NewSNA = builder.CreateProvenanceNoAliasPlain(
                    DepDepI,
                    I->getOperand(Intrinsic::ProvenanceNoAliasNoAliasDeclArg),
                    I->getOperand(Intrinsic::ProvenanceNoAliasIdentifyPArg),
                    I->getOperand(
                        Intrinsic::ProvenanceNoAliasIdentifyPProvenanceArg),
                    I->getOperand(
                        Intrinsic::ProvenanceNoAliasIdentifyPObjIdArg),
                    I->getOperand(Intrinsic::ProvenanceNoAliasScopeArg));
                AAMDNodes Metadata;
                I->getAAMetadata(Metadata);
                NewSNA->setAAMetadata(Metadata);
                I->replaceAllUsesWith(NewSNA);
                I->eraseFromParent();
                Changed = true;
                // And handle the new provenance.noalias for the next sweep
                NextList.push_back(NewSNA);
                continue;
              }
            }
          }
        }

        NextList.push_back(I);
      }

      // 2)  provenance.noaliasA (...), provenance.noaliasB(...)  -->
      // provenance.noaliasA(...)
      {
        for (Instruction *I : NextList) {
          IntrinsicInst *II = cast<IntrinsicInst>(I);
          Instruction *DominatingUse = II;

          ProvenanceWorklist similarProvenances;
          for (User *U : II->getOperand(0)->users()) {
            if (IntrinsicInst *UII = dyn_cast<IntrinsicInst>(U)) {
              if (UII->getParent() && // still valid - ignore already removed
                                      // instructions
                  UII->getIntrinsicID() == Intrinsic::provenance_noalias &&
                  areProvenanceNoAliasCompatible(II, UII)) {
                similarProvenances.push_back(UII);
                if (DT.dominates(UII, DominatingUse))
                  DominatingUse = UII;
              }
            }
          }

          for (Instruction *SI : similarProvenances) {
            if ((SI != DominatingUse) && DT.dominates(DominatingUse, SI)) {
              LLVM_DEBUG(llvm::dbgs() << "-- Collapsing(2):" << *SI << "\n");
              Changed = true;
              SI->replaceAllUsesWith(DominatingUse);
              SI->removeFromParent(); // do not yet erase !
              assert((std::find(NextList.begin(), NextList.end(), SI) !=
                      NextList.end()) &&
                     "Similar ptr_provenance must be on the NextList");
            }
          }
        }

        if (!Changed)
          break;

        // Now eliminate all removed intrinsics
        llvm::erase_if(NextList, [](Instruction *I) {
          if (I->getParent()) {
            return false;
          } else {
            I->deleteValue();
            return true;
          }
        });
      }

      CollapseableProvenanceNoAliasIntrinsics = NextList;
    } while (CollapseableProvenanceNoAliasIntrinsics.size() > 1);
  }
}

// Look at users of llvm.provenance.noalias to find PHI nodes that are used for
// pointer provenance
static void
deduceProvenancePHIs(ProvenanceWorklist &ProvenanceNoAliasIntrinsics,
                     InstructionSet &out_ProvenancePHIs,
                     InstructionSet &out_NoAliasArgGuard,
                     BasicBlockSet &DeadBasicBlocks) {
  LLVM_DEBUG(llvm::dbgs() << "-- Looking up ptr_provenance PHI nodes\n");
  for (Instruction *SNI : ProvenanceNoAliasIntrinsics) {
    ProvenanceWorklist worklist = {SNI};
    while (!worklist.empty()) {
      Instruction *worker = worklist.pop_back_val();
      LLVM_DEBUG(llvm::dbgs() << "worker" << *worker << "\n");
      if (DeadBasicBlocks.count(worker->getParent()))
        continue; // Degenerated llvm-ir; Skip
      for (auto *SNIUser_ : worker->users()) {
        Instruction *SNIUser = dyn_cast<Instruction>(SNIUser_);
        if (SNIUser == nullptr)
          continue;

        if (isa<PHINode>(SNIUser)) {
          // Identify as a ptr_provenance PHI
          if (out_ProvenancePHIs.insert(cast<PHINode>(SNIUser)).second) {
            LLVM_DEBUG(llvm::dbgs() << "--- " << *SNIUser << "\n");
            // and propagate
            worklist.push_back(SNIUser);
          }
        } else if (isa<SelectInst>(SNIUser) || isa<BitCastInst>(SNIUser) ||
                   isa<AddrSpaceCastInst>(SNIUser) ||
                   isa<GetElementPtrInst>(SNIUser)) {
          assert(SNIUser != worker && "not in ssa form ?");
          // look through select/bitcast/addressspacecast
          worklist.push_back(SNIUser);
        } else {
          // load/store/provenance.noalias/arg.guard -> stop looking
          if (auto *CB = dyn_cast<CallBase>(SNIUser)) {
            auto CBIID = CB->getIntrinsicID();
            if (CBIID == Intrinsic::noalias_arg_guard) {
              assert(CB->getOperand(1) == worker &&
                     "a noalias.arg.guard provenance should be linked to "
                     "operand 1");
              out_NoAliasArgGuard.insert(CB);
            } else if (CBIID == Intrinsic::provenance_noalias) {
              // ok
            } else {
              LLVM_DEBUG(llvm::dbgs()
                         << "ERROR: unexpected call/intrinsic depending on "
                            "llvm.provenance.noalias:"
                         << *CB << "\n");
              assert(false &&
                     "Unexpected llvm.provenance.noalias dependency (1)");
            }
          } else {
            if (isa<LoadInst>(SNIUser) || isa<StoreInst>(SNIUser)) {
              // ok
            } else {
              LLVM_DEBUG(llvm::dbgs()
                         << "ERROR: unexpected instruction depending on "
                            "llvm.provenance.noalias:"
                         << *SNIUser << "\n");
              assert(false &&
                     "Unexpected llvm.provenance.noalias dependency (2)");
            }
          }
        }
      }
    }
  }
}

static void RetrieveDeadBasicBlocks(Function &F,
                                    BasicBlockSet &out_DeadBasicBlocks) {
  df_iterator_default_set<BasicBlock *> Reachable;

  // Mark all reachable blocks.
  for (BasicBlock *BB : depth_first_ext(&F, Reachable))
    (void)BB /* Mark all reachable blocks */;

  for (auto &BB : F) {
    if (!Reachable.count(&BB)) {
      out_DeadBasicBlocks.insert(&BB);
      LLVM_DEBUG(llvm::dbgs() << "- Unreachable BB:" << BB.getName() << "\n");
    }
  }

  LLVM_DEBUG(llvm::dbgs() << "- There are " << out_DeadBasicBlocks.size()
                          << " unreachable BB on a total of "
                          << F.getBasicBlockList().size() << "\n");
}

void removeNoAliasIntrinsicsFromDeadBlocks(BasicBlockSet &DeadBlocks) {
  LLVM_DEBUG(llvm::dbgs() << "- removing NoAlias intrinsics from "
                          << DeadBlocks.size() << " dead blocks\n");
  ProvenanceWorklist ToBeRemoved;

  for (auto *BB : DeadBlocks) {
    for (auto &I : *BB) {
      if (auto CB = dyn_cast<CallBase>(&I)) {
        switch (CB->getIntrinsicID()) {
        case Intrinsic::noalias:
        case Intrinsic::noalias_decl:
        case Intrinsic::provenance_noalias:
        case Intrinsic::noalias_arg_guard:
        case Intrinsic::noalias_copy_guard:
          ToBeRemoved.push_back(&I);
          break;
        default:
          break;
        }
      }
    }
  }

  LLVM_DEBUG(llvm::dbgs() << "-- Removing " << ToBeRemoved.size()
                          << " intrinsics\n");
  for (auto *I : ToBeRemoved) {
    I->replaceAllUsesWith(UndefValue::get(I->getType()));
    I->eraseFromParent();
  }
}

bool PropagateAndConvertNoAliasPass::doit(Function &F, DominatorTree &DT) {
  LLVM_DEBUG(llvm::dbgs() << "PropagateAndConvertNoAliasPass:\n");

  // PHASE 0: find interesting instructions
  // - Find all:
  // -- Propagatable noalias intrinsics
  // -- Load instructions
  // -- Store instructions
  ProvenanceWorklist InstructionsForProvenance;
  ProvenanceWorklist LoadStoreIntrinsicInstructions;
  ProvenanceWorklist LookThroughIntrinsics;
  ProvenanceWorklist CollapseableProvenanceNoAliasIntrinsics;
  ValueType2CastMap VT2C;
  InstructionSet ProvenancePHIs;
  ProvenanceWorklist DegeneratedNoAliasAndNoAliasArgGuards;
  ProvenanceWorklist RemainingNoAliasArgGuards;
  InstructionSet DecentNoAliasArgGuards;

  // Do not depend on simplifyCFG or eliminateDeadBlocks. Forcing any of them
  // before the propagate can result in significant code degradations :(
  // Live with the fact that we can observe degenerated llvm-ir.
  BasicBlockSet DeadBasicBlocks;
  RetrieveDeadBasicBlocks(F, DeadBasicBlocks);

  LLVM_DEBUG(llvm::dbgs() << "- gathering intrinsics, stores, loads:\n");
  for (auto &BB : F) {
    if (DeadBasicBlocks.count(&BB))
      continue; // Skip dead basic blocks

    for (auto &I : BB) {
      if (auto CB = dyn_cast<CallBase>(&I)) {
        auto ID = CB->getIntrinsicID();
        if (ID == Intrinsic::noalias) {
          LLVM_DEBUG(llvm::dbgs() << "-- found intrinsic:" << I << "\n");
          auto Op0 = I.getOperand(0);
          if (isa<UndefValue>(Op0)) {
            LLVM_DEBUG(llvm::dbgs() << "--- degenerated\n");
            DegeneratedNoAliasAndNoAliasArgGuards.push_back(&I);
          } else {
            InstructionsForProvenance.push_back(&I);
            LoadStoreIntrinsicInstructions.push_back(&I);
            LookThroughIntrinsics.push_back(&I);
          }
        } else if (ID == Intrinsic::noalias_arg_guard) {
          LLVM_DEBUG(llvm::dbgs() << "-- found intrinsic:" << I << "\n");
          auto Op0 = I.getOperand(0);
          auto Op1 = I.getOperand(1);
          if (isa<UndefValue>(Op0) || isa<UndefValue>(Op1) || (Op0 == Op1)) {
            LLVM_DEBUG(llvm::dbgs() << "--- degenerated\n");
            DegeneratedNoAliasAndNoAliasArgGuards.push_back(&I);
          } else {
            RemainingNoAliasArgGuards.push_back(&I);
          }
        } else if (ID == Intrinsic::provenance_noalias) {
          CollapseableProvenanceNoAliasIntrinsics.push_back(&I);
        }
      } else if (auto LI = dyn_cast<LoadInst>(&I)) {
        LLVM_DEBUG(llvm::dbgs() << "-- found load:" << I << "\n");
        LoadStoreIntrinsicInstructions.push_back(LI);
      } else if (auto SI = dyn_cast<StoreInst>(&I)) {
        LLVM_DEBUG(llvm::dbgs() << "-- found store:" << I << "\n");
        LoadStoreIntrinsicInstructions.push_back(SI);
      }
    }
  }

  // When there are no noalias related intrinsics, don't do anything.
  if (LookThroughIntrinsics.empty() && InstructionsForProvenance.empty() &&
      DegeneratedNoAliasAndNoAliasArgGuards.empty() &&
      CollapseableProvenanceNoAliasIntrinsics.empty() &&
      RemainingNoAliasArgGuards.empty()) {
    LLVM_DEBUG(llvm::dbgs() << "- Nothing to do\n");
    return false;
  }

  if (!DeadBasicBlocks.empty()) {
    removeNoAliasIntrinsicsFromDeadBlocks(DeadBasicBlocks);
  }

  LLVM_DEBUG(
      llvm::dbgs() << "- Looking through degenerated llvm.noalias.arg.guard\n");
  for (Instruction *I : DegeneratedNoAliasAndNoAliasArgGuards) {
    I->replaceAllUsesWith(I->getOperand(0));
    I->eraseFromParent();
  }

  LLVM_DEBUG(llvm::dbgs() << "- Retrieving ptr_provenance PHI nodes and decent "
                             "llvm.noalias.arg.guard\n");
  deduceProvenancePHIs(CollapseableProvenanceNoAliasIntrinsics, ProvenancePHIs,
                       DecentNoAliasArgGuards, DeadBasicBlocks);

  LLVM_DEBUG(
      llvm::dbgs() << "- looking through remaining llvm.noalias.arg.guard");
  for (Instruction *I : RemainingNoAliasArgGuards) {
    if (DecentNoAliasArgGuards.find(I) != DecentNoAliasArgGuards.end()) {
      InstructionsForProvenance.push_back(I);
      LoadStoreIntrinsicInstructions.push_back(I);
      LookThroughIntrinsics.push_back(I);
    } else {
      I->replaceAllUsesWith(I->getOperand(0));
      I->eraseFromParent();
    }
  }

  LLVM_DEBUG(llvm::dbgs() << "- Find out what to do:\n");

  // PHASE 1: forward pass:
  // - Start with all intrinsics
  // -- Track all users
  // -- Interesting users (noalias intrinsics, select, PHI, load/store)
  // -- Do this recursively for users that we can look through
  I2Deps Handled; // instruction -> { dependencies }
  ProvenanceWorklist
      CreationList; // Tracks all keys in Handled, but in a reproducable way
  propagateInstructionsForProvenance(InstructionsForProvenance, Handled,
                                     CreationList, ProvenancePHIs,
                                     DeadBasicBlocks);

  // PHASE 2: add missing load/store/intrinsic instructions:
  for (auto *I : LoadStoreIntrinsicInstructions) {
    if (isa<LoadInst>(I)) {
      if (Handled.insert(I2Deps::value_type(I, {nullptr})).second)
        CreationList.push_back(I);
    } else { // Store or llvm.no_alias
      if (Handled.insert(I2Deps::value_type(I, {nullptr, nullptr})).second)
        CreationList.push_back(I);
    }
  }

#if !defined(NDEBUG)
  auto dumpit = [](I2Deps::value_type &H) {
    auto &out = llvm::dbgs();
    out << *H.first << " -> {";
    bool comma = false;
    for (auto D : H.second) {
      if (comma)
        out << ",";
      comma = true;
      if (D == nullptr) {
        out << "nullptr";
      } else {
        out << *D;
      }
    }
    out << "}\n";
  };
#endif

  // PHASE 3: reconstruct alternative tree
  // - detected dependencies: replace them by new instructions
  // - undetected dependencies: use the original dependency
  // NOTE: See explanation in propagateInstructionsForProvenance for more
  // information !
  LLVM_DEBUG(llvm::dbgs() << "- Reconstructing tree:\n");

  ProvenanceWorklist UnresolvedPHI;
  SmallDenseMap<Instruction *, Value *, 16> I2NewV;
  SmallDenseMap<Instruction *, Value *, 16> I2ArgGuard;

  auto getNewIOrOperand = [&](Instruction *DepOp, Value *OrigOp) {
    assert(((!DepOp) || I2NewV.count(DepOp)) && "DepOp should be known");
    return DepOp ? static_cast<Value *>(I2NewV[DepOp]) : OrigOp;
  };

  // Helper lambda for inserting a new noalias.arg.guard
  auto setNewNoaliasArgGuard = [&](Instruction *I, unsigned Index,
                                   Instruction *DepOp) {
    auto *ProvOp = cast<Instruction>(I2NewV[DepOp]);
    // if we get here, the operand has to be an 'Instruction'
    // (otherwise, DepOp would not be set).
    auto *OpI = cast<Instruction>(I->getOperand(Index));
    auto &ArgGuard = I2ArgGuard[OpI];
    if (ArgGuard == nullptr) {
      // create the instruction close to the origin, so that we don't introduce
      // bad dependencies
      auto InsertionPointIt = OpI->getIterator();
      ++InsertionPointIt;
      if (isa<PHINode>(OpI)) {
        auto End = OpI->getParent()->end();
        while (InsertionPointIt != End) {
          if (!isa<PHINode>(*InsertionPointIt))
            break;
          ++InsertionPointIt;
        }
      }
      IRBuilder<> BuilderForArgs(OpI->getParent(), InsertionPointIt);
      ArgGuard = BuilderForArgs.CreateNoAliasArgGuard(
          OpI, createBitOrPointerOrAddrSpaceCast(ProvOp, OpI->getType(), VT2C),
          OpI->getName() + ".guard");
    }
    I->setOperand(Index, ArgGuard);
  };

  // Map known provenance.noalias that are not handle to themselves
  for (auto SNI : CollapseableProvenanceNoAliasIntrinsics)
    if (Handled.find(SNI) == Handled.end())
      I2NewV[SNI] = SNI;

  // We are doing a number of sweeps. This should always end. Normally the
  // amount of sweeps is low. During initial development, a number of bugs where
  // found by putting a hard limit on the the amount.
  unsigned Watchdog = 1000000; // Only used in assertions build
  (void)Watchdog;
  for (auto CloneableInst : CreationList) {
    assert(Handled.count(CloneableInst) &&
           "Entries in CreationList must also be in Handled");
    assert(!Handled[CloneableInst].empty() &&
           "Only non-empty items should be added to the CreationList");

    LLVM_DEBUG(llvm::dbgs() << "- "; dumpit(*Handled.find(CloneableInst)));
    ProvenanceWorklist Worklist = {CloneableInst};

    while (!Worklist.empty()) {
      Instruction *I = Worklist.back();

      if (I2NewV.count(I)) {
        // already exists - skip
        Worklist.pop_back();
        continue;
      }

      LLVM_DEBUG(llvm::dbgs() << "-- Reconstructing:" << *I << "\n");

      // Check if we have all the needed arguments
      auto HandledIt = Handled.find(I);
      if (HandledIt == Handled.end()) {
        // This can happen after propagation of a llvm.noalias.arg.guard
        Worklist.pop_back();
        I2NewV[I] = I;
        LLVM_DEBUG(llvm::dbgs() << "--- Connected to an existing path!\n");
        continue;
      }

      // If we are a PHI node, just create it
      if (isa<PHINode>(I)) {
        if (ProvenancePHIs.count(cast<PHINode>(I)) == 0) {
          // But only if it is _not_ a ptr_provenance PHI node
          // ======================================== PHI -> { ..... }
          IRBuilder<> Builder(I);
          I2NewV[I] = Builder.CreatePHI(I->getType(), I->getNumOperands(),
                                        Twine("prov.") + I->getName());

          UnresolvedPHI.push_back(I);
        } else {
          I2NewV[I] = I; // Map already existing Provenance PHI to itself
        }
        Worklist.pop_back();
        continue;
      }

      LLVM_DEBUG(llvm::dbgs() << "--- "; dumpit(*HandledIt));
      auto &Deps = HandledIt->second;
      assert((!Deps.empty()) &&
             "Any creatable instruction must have some dependent operands");
      bool canCreateInstruction = true;
      for (auto *DepOp : Deps) {
        if (DepOp != nullptr) {
          if (I2NewV.count(DepOp) == 0) {
            canCreateInstruction = false;
            Worklist.push_back(DepOp);
          }
        }
      }
#if !defined(NDEBUG)
      if (--Watchdog == 0) {
        llvm::errs()
            << "PropagateAndConvertNoAlias: ERROR: WATCHDOG TRIGGERED !\n";
        assert(false && "PropagateAndConvertNoAlias: WATCHDOG TRIGGERED");
      }
#endif
      if (canCreateInstruction) {
        Worklist.pop_back();
        IRBuilder<> Builder(I);

        if (isa<SelectInst>(I)) {
          // ======================================== select -> { lhs, rhs }
          I2NewV[I] = Builder.CreateSelect(
              I->getOperand(0),
              createBitOrPointerOrAddrSpaceCast(
                  getNewIOrOperand(Deps[0], I->getOperand(1)), I->getType(),
                  VT2C),
              createBitOrPointerOrAddrSpaceCast(
                  getNewIOrOperand(Deps[1], I->getOperand(2)), I->getType(),
                  VT2C),
              Twine("prov.") + I->getName());
        } else if (isa<LoadInst>(I)) {
          // ======================================== load -> { ptr }
          LoadInst *LI = cast<LoadInst>(I);

          if (Deps[0]) {
            if (!LI->hasNoaliasProvenanceOperand() ||
                isa<UndefValue>(LI->getNoaliasProvenanceOperand()) ||
                (LI->getPointerOperand() ==
                 LI->getNoaliasProvenanceOperand())) {
              LI->setNoaliasProvenanceOperand(createBitOrPointerOrAddrSpaceCast(
                  I2NewV[Deps[0]], LI->getPointerOperandType(), VT2C));
            } else {
              // nothing to do - propagation should have happend through the
              // provenance !
              // TODO: we might want to add an extra check that the load
              // ptr_provenance was updated
            }
          } else {
            // No extra dependency -> do nothing
            // Note: originally we were adding a 'UndefValue' if there was no
            // ptr_provenance. But that has the same effect as doing nothing.
          }
          I2NewV[I] = I;
        } else if (isa<StoreInst>(I)) {
          // ======================================== store -> { val, ptr }
          StoreInst *SI = cast<StoreInst>(I);

          if (Deps[0]) {
            // We try to store a restrict pointer - restrictness
            Instruction *DepOp = Deps[0];
            setNewNoaliasArgGuard(I, 0, DepOp);
          }
          if (Deps[1]) {
            if (!SI->hasNoaliasProvenanceOperand() ||
                isa<UndefValue>(SI->getNoaliasProvenanceOperand()) ||
                (SI->getPointerOperand() ==
                 SI->getNoaliasProvenanceOperand())) {
              SI->setNoaliasProvenanceOperand(createBitOrPointerOrAddrSpaceCast(
                  I2NewV[Deps[1]], SI->getPointerOperandType(), VT2C));
            } else {
              // nothing to do - propagation should have happend through the
              // provenance !
              // TODO: we might want to add an extra check that the store
              // ptr_provenance was updated
            }
          } else {
            // No extra dependency -> do nothing
            // Note: originally we were adding a 'UndefValue' if there was no
            // ptr_provenance. But that has the same effect as doing nothing.
          }
          I2NewV[I] = I;
        } else if (isa<InsertValueInst>(I)) {
          // We try to insert a restrict pointer into a struct - track it.
          // Track generated noalias_arg_guard also in I2NewI
          assert(Deps.size() == 1 &&
                 "InsertValue tracks exactly one dependency");
          Instruction *DepOp = Deps[0];
          setNewNoaliasArgGuard(I, 1, DepOp);
        } else if (isa<PtrToIntInst>(I)) {
          // We try to convert a restrict pointer to an integer - track it s
          // SROA can produce this.
          // Track generated noalias_arg_guard also in I2NewI
          assert(Deps.size() == 1 &&
                 "InsertValue tracks exactly one dependency");
          Instruction *DepOp = Deps[0];
          setNewNoaliasArgGuard(I, 0, DepOp);
        } else {
          // =============================== ret -> { ...... }
          // =============================== call/invoke/intrinsic -> { ...... }
          auto CB = dyn_cast<CallBase>(I);
          if (CB) {
            assert(CB && "If we get here, we should have a Call");
            switch (CB->getIntrinsicID()) {
            case Intrinsic::noalias: {
              // convert
              assert(Deps.size() == 2);
              Value *IdentifyPProvenance;
              if (Deps[1]) {
                // do the same as with the ptr_provenance in the load
                // instruction
                IdentifyPProvenance = createBitOrPointerOrAddrSpaceCast(
                    I2NewV[Deps[1]],
                    I->getOperand(Intrinsic::NoAliasIdentifyPArg)->getType(),
                    VT2C);
              } else {
                IdentifyPProvenance = UndefValue::get(
                    I->getOperand(Intrinsic::NoAliasIdentifyPArg)->getType());
              }
              Instruction *NewI = Builder.CreateProvenanceNoAliasPlain(
                  getNewIOrOperand(Deps[0], I->getOperand(0)),
                  I->getOperand(Intrinsic::NoAliasNoAliasDeclArg),
                  I->getOperand(Intrinsic::NoAliasIdentifyPArg),
                  IdentifyPProvenance,
                  I->getOperand(Intrinsic::NoAliasIdentifyPObjIdArg),
                  I->getOperand(Intrinsic::NoAliasScopeArg));
              I2NewV[I] = NewI;
              CollapseableProvenanceNoAliasIntrinsics.push_back(NewI);

              // Copy over metadata that is related to the 'getOperand(1)' (aka
              // P)
              AAMDNodes AAMetadata;
              I->getAAMetadata(AAMetadata);
              NewI->setAAMetadata(AAMetadata);
              continue;
            }
            case Intrinsic::noalias_arg_guard: {
              // no update needed - depending llvm.provenance.noalias/gep must
              // have been updated
              continue;
            }
            case Intrinsic::provenance_noalias: {
              // update
              assert((Deps[0] || Deps[1]) &&
                     "provenance.noalias update needs a depending operand");
              if (Deps[0])
                I->setOperand(0, createBitOrPointerOrAddrSpaceCast(
                                     I2NewV[Deps[0]], I->getType(), VT2C));
              if (Deps[1])
                I->setOperand(
                    Intrinsic::ProvenanceNoAliasIdentifyPProvenanceArg,
                    createBitOrPointerOrAddrSpaceCast(
                        I2NewV[Deps[1]],
                        I->getOperand(Intrinsic::ProvenanceNoAliasIdentifyPArg)
                            ->getType(),
                        VT2C));
              I2NewV[I] = I;
              continue;
            }
            default:
              break;
            }
          } else {
            assert(isa<ReturnInst>(I));
          }

          // Introduce a noalias_arg_guard for every argument that is
          // annotated
          assert(I->getNumOperands() == Deps.size());
          for (unsigned i = 0, ci = I->getNumOperands(); i < ci; ++i) {
            Instruction *DepOp = Deps[i];
            if (DepOp) {
              setNewNoaliasArgGuard(I, i, DepOp);
            }
          }
          I2NewV[I] = I;
        }
      }
    }
  }

  // Phase 4: resolve the generated PHI nodes
  LLVM_DEBUG(llvm::dbgs() << "- Resolving " << UnresolvedPHI.size()
                          << " PHI nodes\n");
  for (auto *PHI_ : ProvenancePHIs) {
    PHINode *PHI = cast<PHINode>(PHI_);
    auto it = Handled.find(PHI);
    if (it != Handled.end()) {
      LLVM_DEBUG(llvm::dbgs() << "-- Orig PHI:" << *PHI << "\n");
      auto &Deps = it->second;
      for (unsigned i = 0, ci = Deps.size(); i < ci; ++i) {
        LLVM_DEBUG(if (Deps[i]) llvm::dbgs()
                   << "--- UPDATING:Deps:" << *Deps[i] << "\n");
        Value *IncomingValue = Deps[i] ? I2NewV[Deps[i]] : nullptr;
        if (IncomingValue) {
          if (IncomingValue->getType() != PHI->getType()) {
            IncomingValue = createBitOrPointerOrAddrSpaceCast(
                IncomingValue, PHI->getType(), VT2C);
          }
          LLVM_DEBUG(llvm::dbgs()
                     << "--- IncomingValue:" << *IncomingValue << "\n");
          PHI->setIncomingValue(i, IncomingValue);
        }
      }
      LLVM_DEBUG(llvm::dbgs() << "-- Adapted PHI:" << *PHI << "\n");
    }
  }

  for (auto &PHI : UnresolvedPHI) {
    PHINode *BasePHI = cast<PHINode>(PHI);
    PHINode *NewPHI = cast<PHINode>(I2NewV[PHI]);
    auto &Deps = Handled[PHI];

    LLVM_DEBUG(llvm::dbgs() << "-- Orig PHI:" << *BasePHI << "\n");
    LLVM_DEBUG(llvm::dbgs() << "-- New  PHI:" << *NewPHI << "\n");
    LLVM_DEBUG(llvm::dbgs() << "-- Deps: " << Deps.size() << "\n");
    for (unsigned i = 0, ci = BasePHI->getNumOperands(); i < ci; ++i) {
      auto *BB = BasePHI->getIncomingBlock(i);
      Value *IncomingValue =
          Deps[i] ? I2NewV[Deps[i]] : BasePHI->getIncomingValue(i);
      if (IncomingValue == nullptr) {
        LLVM_DEBUG(llvm::dbgs()
                   << "--- hmm.. operand " << i << " became undef\n");
        IncomingValue = UndefValue::get(NewPHI->getType());
      }
      if (IncomingValue->getType() != NewPHI->getType()) {
        IncomingValue = createBitOrPointerOrAddrSpaceCast(
            IncomingValue, NewPHI->getType(), VT2C);
      }
      NewPHI->addIncoming(IncomingValue, BB);
    }
  }

  // Phase 5: Removing the llvm.noalias
  LLVM_DEBUG(llvm::dbgs() << "- Looking through intrinsics:\n");
  for (Instruction *I : LookThroughIntrinsics) {
    auto CB = dyn_cast<CallBase>(I);
    if (CB->getIntrinsicID() == Intrinsic::noalias ||
        CB->getIntrinsicID() == Intrinsic::noalias_arg_guard) {
      LLVM_DEBUG(llvm::dbgs() << "-- Eliminating: " << *I << "\n");
      I->replaceAllUsesWith(I->getOperand(0));
      I->eraseFromParent();
    } else {
      llvm_unreachable("unhandled lookthrough intrinsic");
    }
  }

  // Phase 6: Collapse llvm.provenance.noalias where possible...
  // - hmm: should we do this as a complete separate pass ??
  collapseProvenanceNoAlias(CollapseableProvenanceNoAliasIntrinsics, DT);

  return true;
}

} // namespace llvm
