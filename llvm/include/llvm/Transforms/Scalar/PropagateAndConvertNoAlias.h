//===- PropagateAndConvertNoAlias.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This pass moves dependencies on llvm.noalias onto the ptr_provenance.
/// It also introduces and propagates provenance.noalias and noalias.arg.guard
/// intrinsics.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_PROPAGATEANDCONVERTNOALIAS_H
#define LLVM_TRANSFORMS_SCALAR_PROPAGATEANDCONVERTNOALIAS_H

#include "llvm/IR/PassManager.h"

namespace llvm {
class DominatorTree;

class PropagateAndConvertNoAliasPass
    : public PassInfoMixin<PropagateAndConvertNoAliasPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

  // Glue for old PM
  bool runImpl(Function &F, llvm::DominatorTree &DT);

private:
  bool doit(Function &F, llvm::DominatorTree &DT);
};
} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_PROPAGATEANDCONVERTNOALIAS_H
