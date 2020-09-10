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

#ifndef LLVM_TRANSFORMS_SCALAR_CONNECTNOALIASDECL_H
#define LLVM_TRANSFORMS_SCALAR_CONNECTNOALIASDECL_H

#include "llvm/IR/PassManager.h"

namespace llvm {
class DominatorTree;

class ConnectNoAliasDeclPass : public PassInfoMixin<ConnectNoAliasDeclPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

  // Glue for old PM
  bool runImpl(Function &F);
};
} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_CONNECTNOALIASDECL_H
