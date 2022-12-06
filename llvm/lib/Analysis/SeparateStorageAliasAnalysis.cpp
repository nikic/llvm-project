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
#include "llvm/InitializePasses.h"

using namespace llvm;

AnalysisKey SeparateStorageAA::Key;

SeparateStorageAAResult SeparateStorageAA::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  return SeparateStorageAAResult();
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
  Result.reset(new SeparateStorageAAResult());
  return false;
}

void SeparateStorageAAWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}
