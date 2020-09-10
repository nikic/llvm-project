//===- ScopedNoAliasAA.cpp - Scoped No-Alias Alias Analysis ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the ScopedNoAlias alias-analysis pass, which implements
// metadata-based scoped no-alias support.
//
// Alias-analysis scopes are defined by an id (which can be a string or some
// other metadata node), a domain node, and an optional descriptive string.
// A domain is defined by an id (which can be a string or some other metadata
// node), and an optional descriptive string.
//
// !dom0 =   metadata !{ metadata !"domain of foo()" }
// !scope1 = metadata !{ metadata !scope1, metadata !dom0, metadata !"scope 1" }
// !scope2 = metadata !{ metadata !scope2, metadata !dom0, metadata !"scope 2" }
//
// Loads and stores can be tagged with an alias-analysis scope, and also, with
// a noalias tag for a specific scope:
//
// ... = load %ptr1, !alias.scope !{ !scope1 }
// ... = load %ptr2, !alias.scope !{ !scope1, !scope2 }, !noalias !{ !scope1 }
//
// When evaluating an aliasing query, if one of the instructions is associated
// has a set of noalias scopes in some domain that is a superset of the alias
// scopes in that domain of some other instruction, then the two memory
// accesses are assumed not to alias.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ScopedNoAliasAA.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "scoped-noalias"

// A handy option for disabling scoped no-alias functionality. The same effect
// can also be achieved by stripping the associated metadata tags from IR, but
// this option is sometimes more convenient.
static cl::opt<bool>
    EnableScopedNoAlias("enable-scoped-noalias", cl::init(true), cl::Hidden,
                        cl::desc("Enable use of scoped-noalias metadata"));

static cl::opt<int>
    MaxNoAliasDepth("scoped-noalias-max-depth", cl::init(12), cl::Hidden,
                    cl::desc("Maximum depth for noalias intrinsic search"));

static cl::opt<int> MaxNoAliasPointerCaptureDepth(
    "scoped-noalias-max-pointer-capture-check", cl::init(320), cl::Hidden,
    cl::desc("Maximum depth for noalias pointer capture search"));

// A 'Undef'as 'NoAliasProvenance'  means 'no known extra information' about
// the pointer provenance. In that case, we need to follow the real pointer
// as it might contain extrainformation provided through llvm.noalias.arg.guard.
// A absent (nullptr) 'NoAliasProvenance', indicates that this access does not
// contain noalias provenance info.
static const Value *selectMemoryProvenance(const MemoryLocation &Loc) {
  if (!Loc.AATags.NoAliasProvenance ||
      isa<UndefValue>(Loc.AATags.NoAliasProvenance))
    return Loc.Ptr;
  return Loc.AATags.NoAliasProvenance; // can be 'nullptr'
}

AliasResult ScopedNoAliasAAResult::alias(const MemoryLocation &LocA,
                                         const MemoryLocation &LocB,
                                         AAQueryInfo &AAQI) {
  if (!EnableScopedNoAlias)
    return AAResultBase::alias(LocA, LocB, AAQI);

  // Get the attached MDNodes.
  const MDNode *AScopes = LocA.AATags.Scope, *BScopes = LocB.AATags.Scope;

  const MDNode *ANoAlias = LocA.AATags.NoAlias, *BNoAlias = LocB.AATags.NoAlias;

  if (!mayAliasInScopes(AScopes, BNoAlias))
    return NoAlias;

  if (!mayAliasInScopes(BScopes, ANoAlias))
    return NoAlias;

  LLVM_DEBUG(llvm::dbgs() << "ScopedNoAliasAAResult::alias\n");
  if (noAliasByIntrinsic(ANoAlias, selectMemoryProvenance(LocA), BNoAlias,
                         selectMemoryProvenance(LocB), nullptr, nullptr, AAQI))
    return NoAlias;

  if (noAliasByIntrinsic(BNoAlias, selectMemoryProvenance(LocB), ANoAlias,
                         selectMemoryProvenance(LocA), nullptr, nullptr, AAQI))
    return NoAlias;

  // If they may alias, chain to the next AliasAnalysis.
  return AAResultBase::alias(LocA, LocB, AAQI);
}

ModRefInfo ScopedNoAliasAAResult::getModRefInfo(const CallBase *Call,
                                                const MemoryLocation &Loc,
                                                AAQueryInfo &AAQI) {
  if (!EnableScopedNoAlias)
    return AAResultBase::getModRefInfo(Call, Loc, AAQI);

  const MDNode *CSNoAlias = Call->getMetadata(LLVMContext::MD_noalias);
  if (!mayAliasInScopes(Loc.AATags.Scope, CSNoAlias))
    return ModRefInfo::NoModRef;

  const MDNode *CSScopes = Call->getMetadata(LLVMContext::MD_alias_scope);
  if (!mayAliasInScopes(CSScopes, Loc.AATags.NoAlias))
    return ModRefInfo::NoModRef;

  LLVM_DEBUG(llvm::dbgs() << "ScopedNoAliasAAResult::getModRefInfo - 1\n");
  if (noAliasByIntrinsic(Loc.AATags.NoAlias, selectMemoryProvenance(Loc),
                         CSNoAlias, nullptr, nullptr, Call, AAQI))
    return ModRefInfo::NoModRef;

  if (noAliasByIntrinsic(CSNoAlias, nullptr, Loc.AATags.NoAlias,
                         selectMemoryProvenance(Loc), Call, nullptr, AAQI))
    return ModRefInfo::NoModRef;

  return AAResultBase::getModRefInfo(Call, Loc, AAQI);
}

ModRefInfo ScopedNoAliasAAResult::getModRefInfo(const CallBase *Call1,
                                                const CallBase *Call2,
                                                AAQueryInfo &AAQI) {
  if (!EnableScopedNoAlias)
    return AAResultBase::getModRefInfo(Call1, Call2, AAQI);

  const MDNode *CS1Scopes = Call1->getMetadata(LLVMContext::MD_alias_scope);
  const MDNode *CS2Scopes = Call2->getMetadata(LLVMContext::MD_alias_scope);
  const MDNode *CS1NoAlias = Call1->getMetadata(LLVMContext::MD_noalias);
  const MDNode *CS2NoAlias = Call2->getMetadata(LLVMContext::MD_noalias);
  if (!mayAliasInScopes(CS1Scopes, Call2->getMetadata(LLVMContext::MD_noalias)))
    return ModRefInfo::NoModRef;

  if (!mayAliasInScopes(CS2Scopes, Call1->getMetadata(LLVMContext::MD_noalias)))
    return ModRefInfo::NoModRef;

  if (noAliasByIntrinsic(CS1NoAlias, nullptr, CS2NoAlias, nullptr, Call1, Call2,
                         AAQI))
    return ModRefInfo::NoModRef;

  if (noAliasByIntrinsic(CS2NoAlias, nullptr, CS1NoAlias, nullptr, Call2, Call1,
                         AAQI))
    return ModRefInfo::NoModRef;

  return AAResultBase::getModRefInfo(Call1, Call2, AAQI);
}

static void collectMDInDomain(const MDNode *List, const MDNode *Domain,
                              SmallPtrSetImpl<const MDNode *> &Nodes) {
  for (const MDOperand &MDOp : List->operands())
    if (const MDNode *MD = dyn_cast<MDNode>(MDOp))
      if (AliasScopeNode(MD).getDomain() == Domain)
        Nodes.insert(MD);
}

bool ScopedNoAliasAAResult::mayAliasInScopes(const MDNode *Scopes,
                                             const MDNode *NoAlias) const {
  if (!Scopes || !NoAlias)
    return true;

  // Collect the set of scope domains relevant to the noalias scopes.
  SmallPtrSet<const MDNode *, 16> Domains;
  for (const MDOperand &MDOp : NoAlias->operands())
    if (const MDNode *NAMD = dyn_cast<MDNode>(MDOp))
      if (const MDNode *Domain = AliasScopeNode(NAMD).getDomain())
        Domains.insert(Domain);

  // We alias unless, for some domain, the set of noalias scopes in that domain
  // is a superset of the set of alias scopes in that domain.
  for (const MDNode *Domain : Domains) {
    SmallPtrSet<const MDNode *, 16> ScopeNodes;
    collectMDInDomain(Scopes, Domain, ScopeNodes);
    if (ScopeNodes.empty())
      continue;

    SmallPtrSet<const MDNode *, 16> NANodes;
    collectMDInDomain(NoAlias, Domain, NANodes);

    // To not alias, all of the nodes in ScopeNodes must be in NANodes.
    bool FoundAll = true;
    for (const MDNode *SMD : ScopeNodes)
      if (!NANodes.count(SMD)) {
        FoundAll = false;
        break;
      }

    if (FoundAll)
      return false;
  }

  return true;
}

bool ScopedNoAliasAAResult::findCompatibleNoAlias(
    const Value *P, const MDNode *ANoAlias, const MDNode *BNoAlias,
    const DataLayout &DL, SmallPtrSetImpl<const Value *> &Visited,
    SmallVectorImpl<Instruction *> &CompatibleSet, int Depth) {
  // When a pointer is derived from multiple noalias calls, there are two
  // potential reasons:
  //   1. The path of derivation is uncertain (because of a select, PHI, etc.).
  //   2. Some noalias calls are derived from other noalias calls.
  // Logically, we need to treat (1) as an "and" and (2) as an "or" when
  // checking for scope compatibility. If we don't know from which noalias call
  // a pointer is derived, then we need to require compatibility with all of
  // them. If we're derived from a noalias call that is derived from another
  // noalias call, then we need the ability to effectively ignore the inner one
  // in favor of the outer one (thus, we only need compatibility with one or
  // the other).
  //
  // Scope compatibility means that, as with the noalias metadata, within each
  // domain, the set of noalias intrinsic scopes is a subset of the noalias
  // scopes.
  //
  // Given this, we check compatibility of the relevant sets of noalias calls
  // from which LocA.Ptr might derive with both LocA.AATags.NoAlias and
  // LocB.AATags.NoAlias, and LocB.Ptr does not derive from any of the noalias
  // calls in some set, then we can conclude NoAlias.
  //
  // So if we have:
  //   noalias1  noalias3
  //      |         |
  //   noalias2  noalias4
  //      |         |
  //       \       /
  //        \     /
  //         \   /
  //          \ /
  //         select
  //           |
  //        noalias5
  //           |
  //        noalias6
  //           |
  //          PtrA
  //
  //  - If PtrA is compatible with noalias6, and PtrB is also compatible,
  //    but does not derive from noalias6, then NoAlias.
  //  - If PtrA is compatible with noalias5, and PtrB is also compatible,
  //    but does not derive from noalias5, then NoAlias.
  //  - If PtrA is compatible with noalias2 and noalias4, and PtrB is also
  //    compatible, but does not derive from either, then NoAlias.
  //  - If PtrA is compatible with noalias2 and noalias3, and PtrB is also
  //    compatible, but does not derive from either, then NoAlias.
  //  - If PtrA is compatible with noalias1 and noalias4, and PtrB is also
  //    compatible, but does not derive from either, then NoAlias.
  //  - If PtrA is compatible with noalias1 and noalias3, and PtrB is also
  //    compatible, but does not derive from either, then NoAlias.
  //
  //  We don't need, or want, to explicitly build N! sets to check for scope
  //  compatibility. Instead, recurse through the tree of underlying objects.

  SmallVector<Instruction *, 8> NoAliasCalls;
  P = getUnderlyingObject(P, 0, &NoAliasCalls);

  // If we've already visited this underlying value (likely because this is a
  // PHI that depends on itself, directly or indirectly), we must not have
  // returned false the first time, so don't do so this time either.
  if (!Visited.insert(P).second)
    return true;

  auto getNoAliasScopeMDNode = [](IntrinsicInst *II) {
    return dyn_cast<MDNode>(
        cast<MetadataAsValue>(
            II->getOperand(II->getIntrinsicID() == Intrinsic::provenance_noalias
                               ? Intrinsic::ProvenanceNoAliasScopeArg
                               : Intrinsic::NoAliasScopeArg))
            ->getMetadata());
  };

  // Our pointer is derived from P, with NoAliasCalls along the way.
  // Compatibility with any of them is fine.
  auto NAI = find_if(NoAliasCalls, [&](Instruction *A) {
    return !mayAliasInScopes(getNoAliasScopeMDNode(cast<IntrinsicInst>(A)),
                             ANoAlias) &&
           !mayAliasInScopes(getNoAliasScopeMDNode(cast<IntrinsicInst>(A)),
                             BNoAlias);
  });
  if (NAI != NoAliasCalls.end()) {
    CompatibleSet.push_back(*NAI);
    return true;
  }

  // We've not found a compatible noalias call, but we might be able to keep
  // looking. If this underlying object is really a PHI or a select, we can
  // check the incoming values. They all need to be compatible, and if so, we
  // can take the union of all of the compatible noalias calls as the set to
  // return for further validation.
  SmallVector<const Value *, 8> Children;
  if (const auto *SI = dyn_cast<SelectInst>(P)) {
    Children.push_back(SI->getTrueValue());
    Children.push_back(SI->getFalseValue());
  } else if (const auto *PN = dyn_cast<PHINode>(P)) {
    for (Value *IncommingValue : PN->incoming_values())
      Children.push_back(IncommingValue);
  }

  if (Children.empty() || Depth == MaxNoAliasDepth)
    return false;

  SmallPtrSet<const Value *, 16> ChildVisited;
  SmallVector<Instruction *, 8> ChildCompatSet;
  for (auto &C : Children) {
    ChildVisited.clear();
    ChildVisited.insert(Visited.begin(), Visited.end());
    ChildVisited.insert(P);

    ChildCompatSet.clear();
    if (!findCompatibleNoAlias(C, ANoAlias, BNoAlias, DL, ChildVisited,
                               ChildCompatSet, Depth + 1))
      return false;

    CompatibleSet.insert(CompatibleSet.end(), ChildCompatSet.begin(),
                         ChildCompatSet.end());
  }

  // All children were compatible, and we've added them to CompatibleSet.
  return true;
}

bool ScopedNoAliasAAResult::noAliasByIntrinsic(
    const MDNode *ANoAlias, const Value *APtr, const MDNode *BNoAlias,
    const Value *BPtr, const CallBase *CallA, const CallBase *CallB,
    AAQueryInfo &AAQI) {
  LLVM_DEBUG(llvm::dbgs() << ">ScopedNoAliasAAResult::noAliasByIntrinsic:{"
                          << (const void *)ANoAlias << "," << (const void *)APtr
                          << "},{" << (const void *)BNoAlias << ","
                          << (const void *)BPtr << "}\n");
  if (!ANoAlias || !BNoAlias)
    return false;

  if (CallA) {
    // We're querying a callsite against something else, where we want to know
    // if the callsite (CallA) is derived from some noalias call(s) and the
    // other thing is not derived from those noalias call(s). This can be
    // determined only if CallA only accesses memory through its arguments.
    FunctionModRefBehavior MRB = getModRefBehavior(CallA);
    if (MRB != FMRB_OnlyAccessesArgumentPointees &&
        MRB != FMRB_OnlyReadsArgumentPointees)
      return false;

    LLVM_DEBUG(dbgs() << "SNA: CSA: " << *CallA << "\n");
    // Since the memory-access behavior of CallA is determined only by its
    // arguments, we can answer this query in the affirmative if we can prove a
    // lack of aliasing for all pointer arguments.
    for (Value *Arg : CallA->args()) {
      if (!Arg->getType()->isPointerTy())
        continue;

      if (!noAliasByIntrinsic(ANoAlias, Arg, BNoAlias, BPtr, nullptr, CallB,
                              AAQI)) {
        LLVM_DEBUG(dbgs() << "SNA: CSA: noalias fail for arg: " << *Arg
                          << "\n");
        return false;
      }
    }

    return true;
  }

  const auto *AInst = dyn_cast<Instruction>(APtr);
  if (!AInst || !AInst->getParent())
    return false;
  const DataLayout &DL = AInst->getParent()->getModule()->getDataLayout();

  if (!CallB && !BPtr)
    return false;

  LLVM_DEBUG(dbgs() << "SNA: A: " << *APtr << "\n");
  LLVM_DEBUG(dbgs() << "SNB: "; if (CallB) dbgs() << "CSB: " << *CallB;
             else if (BPtr) dbgs() << "B: " << *BPtr;
             else dbgs() << "B: nullptr"; dbgs() << "\n");

  SmallPtrSet<const Value *, 8> Visited;
  SmallVector<Instruction *, 8> CompatibleSet;
  if (!findCompatibleNoAlias(APtr, ANoAlias, BNoAlias, DL, Visited,
                             CompatibleSet))
    return false;

  assert(!CompatibleSet.empty() &&
         "Fould an empty set of compatible intrinsics?");

  LLVM_DEBUG(dbgs() << "SNA: Found a compatible set!\n");
#ifndef NDEBUG
  for (auto &C : CompatibleSet)
    LLVM_DEBUG(dbgs() << "\t" << *C << "\n");
  LLVM_DEBUG(dbgs() << "\n");
#endif

  // We have a set of compatible noalias calls (compatible with the scopes from
  // both LocA and LocB) from which LocA.Ptr potentially derives. We now need
  // to make sure that LocB.Ptr does not derive from any in that set. For
  // correctness, there cannot be a depth limit here (if a pointer is derived
  // from a noalias call, we must know).
  SmallVector<const Value *, 8> BObjs;
  SmallVector<Instruction *, 8> BNoAliasCalls;
  if (CallB) {
    for (Value *Arg : CallB->args())
      getUnderlyingObjects(Arg, BObjs, nullptr, 0, &BNoAliasCalls);
  } else {
    getUnderlyingObjects(const_cast<Value *>(BPtr), BObjs, nullptr, 0,
                         &BNoAliasCalls);
  }

  LLVM_DEBUG(dbgs() << "SNA: B/CSB noalias:\n");
#ifndef NDEBUG
  for (auto &B : BNoAliasCalls)
    LLVM_DEBUG(dbgs() << "\t" << *B << "\n");
  LLVM_DEBUG(dbgs() << "\n");
#endif

  // We need to check now if any compatible llvm.provenance.noalias call is
  // potentially using the same 'P' object as one of the 'BNoAliasCalls'.
  // If this is true for at least one entry, we must bail out and assume
  // 'may_alias'

  {
    MDNode *NoAliasUnknownScopeMD =
        AInst->getParent()->getParent()->getMetadata("noalias");

    const struct {
      unsigned IdentifyPObjIdArg;
      unsigned ScopeArg;
      unsigned IdentifyPArg;
    } ProvNoAlias[2] = {{Intrinsic::NoAliasIdentifyPObjIdArg,
                         Intrinsic::NoAliasScopeArg,
                         Intrinsic::NoAliasIdentifyPArg},
                        {Intrinsic::ProvenanceNoAliasIdentifyPObjIdArg,
                         Intrinsic::ProvenanceNoAliasScopeArg,
                         Intrinsic::ProvenanceNoAliasIdentifyPArg}};
    for (Instruction *CA : CompatibleSet) {
      LLVM_DEBUG(llvm::dbgs() << "- CA:" << *CA << "\n");
      assert(isa<IntrinsicInst>(CA) &&
             (cast<IntrinsicInst>(CA)->getIntrinsicID() ==
                  llvm::Intrinsic::provenance_noalias ||
              cast<IntrinsicInst>(CA)->getIntrinsicID() ==
                  llvm::Intrinsic::noalias));
      const int CA_IsProv = cast<IntrinsicInst>(CA)->getIntrinsicID() ==
                                    llvm::Intrinsic::provenance_noalias
                                ? 1
                                : 0;
      for (Instruction *CB : BNoAliasCalls) {
        LLVM_DEBUG(llvm::dbgs() << "- CB:" << *CB << "\n");
        assert(isa<IntrinsicInst>(CB) &&
               (cast<IntrinsicInst>(CB)->getIntrinsicID() ==
                    llvm::Intrinsic::provenance_noalias ||
                cast<IntrinsicInst>(CB)->getIntrinsicID() ==
                    llvm::Intrinsic::noalias));
        const int CB_IsProv = cast<IntrinsicInst>(CB)->getIntrinsicID() ==
                                      llvm::Intrinsic::provenance_noalias
                                  ? 1
                                  : 0;

        // With the llvm.provenance.noalias version, we have different parts
        // that can represent a P:
        // - the actual 'identifyP' address (or an offset vs an optimized away
        //   alloca)
        // - the objectId (objectId's currently represent an offset to the
        //   original alloca of the object)
        // - the scope (different scopes = different objects; with the exception
        //   of the 'unknown scope' an unknown scope can potentially be the same
        //   as a real variable scope.
        // If any of these are different, the P will not alias => *P will also
        // not alias

        // Let's start with the fast checks first;

        // Same call ?
        if (CA == CB) {
          LLVM_DEBUG(llvm::dbgs() << "SNA == SNB\n");
          return false;
        }

        // - different objectId ?
        {
          // check ObjId first: if the obj id's (aka, offset in the object) are
          // different, they represent different objects
          auto ObjIdA =
              cast<ConstantInt>(
                  CA->getOperand(ProvNoAlias[CA_IsProv].IdentifyPObjIdArg))
                  ->getZExtValue();
          auto ObjIdB =
              cast<ConstantInt>(
                  CB->getOperand(ProvNoAlias[CB_IsProv].IdentifyPObjIdArg))
                  ->getZExtValue();
          if (ObjIdA != ObjIdB) {
            LLVM_DEBUG(llvm::dbgs() << "SNA.ObjId != SNB.ObjId\n");
            continue;
          }
        }

        // Different Scope ? (except for unknown scope)
        {
          bool isDifferentPByScope = true;
          auto *CASnaScope = CA->getOperand(ProvNoAlias[CA_IsProv].ScopeArg);
          auto *CBSnaScope = CB->getOperand(ProvNoAlias[CB_IsProv].ScopeArg);
          if (CASnaScope == CBSnaScope) {
            // compatibility check below will resolve
            isDifferentPByScope = false;
          } else {
            if (NoAliasUnknownScopeMD) {
              if ((cast<MetadataAsValue>(CASnaScope)->getMetadata() ==
                   NoAliasUnknownScopeMD) ||
                  (cast<MetadataAsValue>(CBSnaScope)->getMetadata() ==
                   NoAliasUnknownScopeMD)) {
                isDifferentPByScope = false;
              }
            }
          }
          if (isDifferentPByScope) {
            LLVM_DEBUG(llvm::dbgs()
                       << "SNA.Scope != SNB.Scope (and not 'unknown scope')\n");
            continue;
          }
        }

        // Different 'P' ?
        {
          Value *P_A = CA->getOperand(ProvNoAlias[CA_IsProv].IdentifyPArg);
          Value *P_B = CB->getOperand(ProvNoAlias[CA_IsProv].IdentifyPArg);

          if (P_A == P_B) {
            LLVM_DEBUG(dbgs() << " SNA.Scope == SNB.Scope, SNA.P == SNB.P\n");
            return false;
          }

          if (auto *CP_A = dyn_cast<Constant>(P_A)) {
            if (auto *CP_B = dyn_cast<Constant>(P_B)) {
              CP_B = ConstantExpr::getBitCast(CP_B, CP_A->getType());
              Constant *Cmp =
                  ConstantExpr::getCompare(CmpInst::ICMP_NE, CP_A, CP_B, true);
              if (Cmp && Cmp->isNullValue()) {
                LLVM_DEBUG(dbgs() << " SNA.Scope == SNB.Scope, !(SNA.P != "
                                     "SNB.P) as constant\n");
                return false;
              }
            }
          }
          // Check if P_A.addr and P_B.addr alias. If they don't, they describe
          // different pointers.
          LLVM_DEBUG(llvm::dbgs()
                     << " SNA.P=" << *P_A << ", SNB.P=" << *P_B << "\n");
          AAMDNodes P_A_Metadata;
          AAMDNodes P_B_Metadata;
          CA->getAAMetadata(P_A_Metadata);
          CB->getAAMetadata(P_B_Metadata);

          // The ptr_provenance is not handled in the
          // Instruction::getAAMetadata for intrinsics
          if (CA_IsProv) {
            P_A_Metadata.NoAliasProvenance = CA->getOperand(
                Intrinsic::ProvenanceNoAliasIdentifyPProvenanceArg);
          }
          if (CB_IsProv) {
            P_B_Metadata.NoAliasProvenance = CB->getOperand(
                Intrinsic::ProvenanceNoAliasIdentifyPProvenanceArg);
          }

          // Check with 1 unit
          MemoryLocation ML_P_A(P_A, 1ull, P_A_Metadata);
          MemoryLocation ML_P_B(P_B, 1ull, P_B_Metadata);
          if (getBestAAResults().alias(ML_P_A, ML_P_B, AAQI) !=
              AliasResult::NoAlias) {
            LLVM_DEBUG(llvm::dbgs() << " P ... may alias\n");
            return false;
          }
          LLVM_DEBUG(llvm::dbgs() << " P is NoAlias\n");
          continue;
        }
      }
    }
  }

  // The noalias scope from the compatible intrinsics are really identified by
  // their scope argument, and we need to make sure that LocB.Ptr is not only
  // not derived from the calls currently in CompatibleSet, but also from any
  // other intrinsic with the same scope. We can't just search the list of
  // noalias intrinsics in BNoAliasCalls because we care not just about those
  // direct dependence, but also dependence through capturing. Metadata
  // do not have use lists, but MetadataAsValue objects do (and they are
  // uniqued), so we can search their use list. As a result, however,
  // correctness demands that the scope list has only one element (so that we
  // can find all uses of that scope by noalias intrinsics by looking at the
  // use list of the associated scope list).
  SmallPtrSet<Instruction *, 8> CompatibleSetMembers(CompatibleSet.begin(),
                                                     CompatibleSet.end());
  SmallVector<MetadataAsValue *, 8> CompatibleSetMVs;
  for (auto &C : CompatibleSet) {
    CompatibleSetMVs.push_back(cast<MetadataAsValue>(
        C->getOperand(cast<IntrinsicInst>(C)->getIntrinsicID() ==
                              Intrinsic::provenance_noalias
                          ? Intrinsic::ProvenanceNoAliasScopeArg
                          : Intrinsic::NoAliasScopeArg)));
  }
  for (auto &MV : CompatibleSetMVs)
    for (Use &U : MV->uses())
      if (auto *UI = dyn_cast<Instruction>(U.getUser())) {
        // Skip noalias declarations
        if (auto *CB = dyn_cast<CallBase>(UI))
          if (CB->getIntrinsicID() == Intrinsic::noalias_decl)
            continue;
        if (CompatibleSetMembers.insert(UI).second) {
          CompatibleSet.push_back(UI);
          LLVM_DEBUG(dbgs() << "SNA: Adding to compatible set based on MD use: "
                            << *UI << "\n");
        }
      }

  LLVM_DEBUG(dbgs() << "SNA: B does not derive from the compatible set!\n");

  // Note: This can be removed when legacy-pass-manager support is removed;
  // BasicAA always has a DT available, and only under the hack where this is
  // an immutable pass, not a function pass, might we not have one.
  LLVM_DEBUG(dbgs() << "SNA: DT is " << (DT ? "available" : "unavailable")
                    << "\n");

  // We now know that LocB.Ptr does not derive from any of the noalias calls in
  // CompatibleSet directly. We do, however, need to make sure that it cannot
  // derive from them by capture.
  for (auto &V : BObjs) {
    // If the underlying object is not an instruction, then it can't be
    // capturing the output value of an instruction (specifically, the noalias
    // intrinsic call), and we can ignore it.
    auto *I = dyn_cast<Instruction>(V);
    if (!I)
      continue;
    if (isIdentifiedFunctionLocal(I))
      continue;

    LLVM_DEBUG(dbgs() << "SNA: Capture check for B/CSB UO: " << *I << "\n");

    // If the value from the noalias intrinsic has been captured prior to the
    // instruction defining the underlying object, then LocB.Ptr might yet be
    // derived from the return value of the noalias intrinsic, and we cannot
    // conclude anything about the aliasing.
    for (auto &C : CompatibleSet)
      if (PointerMayBeCapturedBefore(C, /* ReturnCaptures */ false,
                                     /* StoreCaptures */ false, I, DT,
                                     /* IncludeI */ false,
                                     MaxNoAliasPointerCaptureDepth)) {
        LLVM_DEBUG(dbgs() << "SNA: Pointer " << *C << " might be captured!\n");
        return false;
      }
  }

  if (CallB) {
    FunctionModRefBehavior MRB = getModRefBehavior(CallB);
    if (MRB != FMRB_OnlyAccessesArgumentPointees &&
        MRB != FMRB_OnlyReadsArgumentPointees) {
      // If we're querying against a callsite, and it might read from memory
      // not based on its arguments, then we need to check whether or not the
      // relevant noalias results have been captured prior to the callsite.
      for (auto &C : CompatibleSet)
        if (PointerMayBeCapturedBefore(C, /* ReturnCaptures */ false,
                                       /* StoreCaptures */ false, CallB, DT)) {
          LLVM_DEBUG(dbgs()
                     << "SNA: CSB: Pointer " << *C << " might be captured!\n");
          return false;
        }
    }
  }

  LLVM_DEBUG(dbgs() << " SNA: noalias!\n");
  return true;
}

AnalysisKey ScopedNoAliasAA::Key;

ScopedNoAliasAAResult ScopedNoAliasAA::run(Function &F,
                                           FunctionAnalysisManager &AM) {
  return ScopedNoAliasAAResult(&AM.getResult<DominatorTreeAnalysis>(F));
}

char ScopedNoAliasAAWrapperPass::ID = 0;

INITIALIZE_PASS(ScopedNoAliasAAWrapperPass, "scoped-noalias-aa",
                "Scoped NoAlias Alias Analysis", false, true)

ImmutablePass *llvm::createScopedNoAliasAAWrapperPass() {
  return new ScopedNoAliasAAWrapperPass();
}

ScopedNoAliasAAWrapperPass::ScopedNoAliasAAWrapperPass() : ImmutablePass(ID) {
  initializeScopedNoAliasAAWrapperPassPass(*PassRegistry::getPassRegistry());
}

bool ScopedNoAliasAAWrapperPass::doInitialization(Module &M) {
  Result.reset(new ScopedNoAliasAAResult(nullptr));
  return false;
}

bool ScopedNoAliasAAWrapperPass::doFinalization(Module &M) {
  Result.reset();
  return false;
}

void ScopedNoAliasAAWrapperPass::setDT() {
  if (auto *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>())
    Result->setDT(&DTWP->getDomTree());
}

void ScopedNoAliasAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addUsedIfAvailable<AAResultsWrapperPass>();
}
