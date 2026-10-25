//===- PrepareEdgeAddress.cpp - Prepare edgeaddress uses for codegen ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The value of edgeaddress(@f, N) is the address of the destination of the
// indirectswitch case using it. Instruction selection and the AsmPrinter lower
// it to the address of that (address-taken) block, but the destination is only
// final once all IR passes of the code generation pipeline have run on @f:
// e.g. CodeGenPrepare may split the indirectswitch edge. Global initializers
// are only emitted after all functions are code generated, so they are not
// affected. Code in other functions, however, may be code generated before @f.
//
// This pass runs at the start of the code generation pipeline and replaces
// uses of edgeaddresses in instructions of functions other than their own with
// loads from private globals initialized with the edgeaddress. Edgeaddresses
// that are not a case value of any indirectswitch have an unspecified value,
// and are replaced with the address of their function (which is also what
// instruction selection would use).
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/PrepareEdgeAddress.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ReplaceConstant.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

using namespace llvm;

#define DEBUG_TYPE "prepare-edgeaddress"

static bool isIndirectSwitchCaseUse(const Use &U) {
  return isa<IndirectSwitchInst>(U.getUser()) && U.getOperandNo() != 0;
}

/// Collect the functions other than EA's own function that use \p EA in an
/// instruction, directly or through constant expressions or aggregates.
static void collectForeignUsers(EdgeAddress *EA,
                                SmallSetVector<Function *, 4> &Fns) {
  SmallVector<User *, 8> Worklist(EA->users());
  SmallPtrSet<User *, 8> Visited;
  while (!Worklist.empty()) {
    User *U = Worklist.pop_back_val();
    if (!Visited.insert(U).second)
      continue;
    if (auto *I = dyn_cast<Instruction>(U)) {
      if (I->getFunction() != EA->getFunction())
        Fns.insert(I->getFunction());
    } else if (isa<ConstantExpr, ConstantAggregate>(U)) {
      append_range(Worklist, U->users());
    }
  }
}

static bool prepareEdgeAddresses(Module &M) {
  bool Changed = false;
  for (Function &F : M) {
    if (!F.hasEdgeAddress())
      continue;
    for (EdgeAddress *EA : F.getEdgeAddresses()) {
      SmallSetVector<Function *, 4> Fns;
      collectForeignUsers(EA, Fns);
      if (Fns.empty())
        continue;

      // Without an indirectswitch case, the value is unspecified and does not
      // depend on code generation.
      if (none_of(EA->uses(), isIndirectSwitchCaseUse)) {
        EA->replaceAllUsesWith(&F);
        Changed = true;
        continue;
      }

      // Turn constant expressions using EA into instructions, so that EA is
      // directly used by instructions.
      Constant *C = EA;
      for (Function *G : Fns)
        convertUsersOfConstantsToInstructions(C, G,
                                              /*RemoveDeadConstants=*/false);

      // The global is not marked constant, so that loads from it are not
      // folded back to the edgeaddress.
      auto *GV = new GlobalVariable(
          M, EA->getType(), /*isConstant=*/false, GlobalValue::PrivateLinkage,
          EA, F.getName() + ".edgeaddress." + Twine(EA->getID()));
      GV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      for (Use &U : make_early_inc_range(EA->uses())) {
        auto *I = dyn_cast<Instruction>(U.getUser());
        if (!I || I->getFunction() == &F)
          continue;
        Instruction *InsertPt = I;
        if (auto *PN = dyn_cast<PHINode>(I))
          InsertPt = PN->getIncomingBlock(U)->getTerminator();
        auto *Load = new LoadInst(EA->getType(), GV, GV->getName() + ".load",
                                  InsertPt->getIterator());
        U.set(Load);
      }
      Changed = true;
    }
  }
  return Changed;
}

PreservedAnalyses PrepareEdgeAddressPass::run(Module &M,
                                              ModuleAnalysisManager &MAM) {
  if (!prepareEdgeAddresses(M))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

namespace {

class PrepareEdgeAddressLegacyPass : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  PrepareEdgeAddressLegacyPass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  bool runOnModule(Module &M) override { return prepareEdgeAddresses(M); }
};

} // end anonymous namespace

char PrepareEdgeAddressLegacyPass::ID = 0;

INITIALIZE_PASS(PrepareEdgeAddressLegacyPass, DEBUG_TYPE,
                "Prepare edgeaddress uses for code generation", false, false)

ModulePass *llvm::createPrepareEdgeAddressPass() {
  return new PrepareEdgeAddressLegacyPass();
}
