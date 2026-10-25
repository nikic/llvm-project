//===- llvm/CodeGen/PrepareEdgeAddress.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Prepare uses of edgeaddress constants in functions other than their own for
/// code generation.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_PREPAREEDGEADDRESS_H
#define LLVM_CODEGEN_PREPAREEDGEADDRESS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

/// Replace uses of edgeaddress constants in functions other than their own with
/// loads from private globals initialized with the edgeaddress.
class PrepareEdgeAddressPass
    : public RequiredPassInfoMixin<PrepareEdgeAddressPass> {
public:
  LLVM_ABI PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

} // namespace llvm

#endif // LLVM_CODEGEN_PREPAREEDGEADDRESS_H
