//===- ConnectNoAliasDecl.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This pass connects provenance.noalias intrinsics to the corresponding
/// llvm.noalias.decl, based on the alloca of the pointer.
///
//===----------------------------------------------------------------------===//
//
// When the original restrict declaration is not directly available,
// llvm.noalias, llvm.provenance.noalias and llvm.noalias.copy.guard can be
// associated with an 'unknown' (out of function) noalias scope. After certain
// optimizations, like SROA, inlining, ... it is possible that a
// llvm.noalias.decl is associated to an alloca, to which a llvm.noalias,
// llvm.provenance.noalias orllvm.noalias.copy.guard intrinsics is also
// associated. When the latter intrinsics are still refering to the 'unknown'
// scope, we can now refine the information by associating the llvm.noalias.decl
// and its information to the other noalias intrinsics that are depending on the
// same alloca.
//
// This pass will connect those llvm.noalias.decl to those
// llvm.noalias,llvm.provenance.noalias and llvm.noalias.copy.guard. It will
// also propagate the embedded information.
//
// This pass is best placed before SROA or PropagateAndConvertNoAlias.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/ConnectNoAliasDecl.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/IR/Dominators.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace {
class ConnectNoAliasDeclLegacyPass : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  ConnectNoAliasDeclLegacyPass() : FunctionPass(ID) {
    initializeConnectNoAliasDeclLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // FIXME: is all of this valid ?
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.addPreserved<CallGraphWrapperPass>(); // FIXME: not sure this is
                                             // valid. It ensures the same pass
                                             // order as if this pass was not
                                             // there
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequiredTransitive<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
  }

private:
  ConnectNoAliasDeclPass Impl;
};
} // namespace

char ConnectNoAliasDeclLegacyPass::ID = 0;
INITIALIZE_PASS_BEGIN(
    ConnectNoAliasDeclLegacyPass, "connect-noaliasdecl",
    "Connect llvm.noalias.decl", false,
    false) //  to llvm.noalias/llvm.provenance.noalias/llvm.noalias.copy.guard
           //  intrinsics
INITIALIZE_PASS_END(
    ConnectNoAliasDeclLegacyPass, "connect-noaliasdecl",
    "Connect llvm.noalias.decl", false,
    false) //  to llvm.noalias/llvm.provenance.noalias/llvm.noalias.copy.guard
           //  intrinsics

bool ConnectNoAliasDeclLegacyPass::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  return Impl.runImpl(F);
}

namespace llvm {

bool ConnectNoAliasDeclPass::runImpl(Function &F) {
  return llvm::propagateAndConnectNoAliasDecl(&F);
}

PreservedAnalyses ConnectNoAliasDeclPass::run(Function &F,
                                              FunctionAnalysisManager &AM) {
  bool Changed = runImpl(F);

  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<GlobalsAA>();
  //?? PA.preserve<CallGraphWrapperPass>(); // FIXME: not sure this is valid,
  // see above

  return PA;
}

FunctionPass *createConnectNoAliasDeclPass() {
  return new ConnectNoAliasDeclLegacyPass();
}
} // namespace llvm
