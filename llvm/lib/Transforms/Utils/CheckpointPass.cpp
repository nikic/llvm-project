//===-- CheckpointPass.cpp - Checkpoint save/rollback using passes --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/CheckpointPass.h"

using namespace llvm;

PreservedAnalyses CheckpointSavePass::run(Module &M,
                                          ModuleAnalysisManager &AM) {
  M.getContext().getChkpntEngine().startTracking(
      /*RunVerifier=*/true, /*MaxNumOfTrackedChanges=*/1048576);
  return PreservedAnalyses::all();
}

PreservedAnalyses CheckpointAcceptPass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  M.getContext().getChkpntEngine().accept();
  return PreservedAnalyses::all();
}

PreservedAnalyses CheckpointRollbackPass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  M.getContext().getChkpntEngine().rollback();
  return PreservedAnalyses::all();
}
