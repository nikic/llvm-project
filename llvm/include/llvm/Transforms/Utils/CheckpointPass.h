//===-- CheckpointPass.h - Checkpoint save/rollback using passes-*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A simple pass wrapper for Checkpointing.
//
// This allows you to save before a pass and accept/rollback after it, like:
//   opt -passes=checkpoint-save,<some passes>,checkpoint-rollback input.ll
// Which should generate the same IR as input.ll.
//
// It is currently used for stress-testing the Checkpointing infrastructure, but
// it could also be useful for trying out a series of local optimizations and
// reverting them if they don't prove better than the original code.
//
// Please note that nested checkpointing is not currently supported, so if any
// of the passes in between checkpoint-save and checkpoint-accept/rollback are
// already using checkpointing, then this will cause a crash.

#ifndef LLVM_TRANSFORMS_UTILS_CHECKPOINTPASS_H
#define LLVM_TRANSFORMS_UTILS_CHECKPOINTPASS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

class CheckpointSavePass : public PassInfoMixin<CheckpointSavePass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

class CheckpointAcceptPass : public PassInfoMixin<CheckpointAcceptPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

class CheckpointRollbackPass : public PassInfoMixin<CheckpointRollbackPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};
} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_CHECKPOINTPASS_H
