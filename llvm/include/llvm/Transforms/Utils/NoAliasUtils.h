//===- llvm/Transforms/Utils/NoAliasUtils.h - NoAlias utilities -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines utilities for noalias metadata and intrinsics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_NOALIASUTILS_H
#define LLVM_TRANSFORMS_UTILS_NOALIASUTILS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

namespace llvm {
class Function;

/// Connect llvm.noalias.decl to noalias/provenance.noalias intrinsics that are
/// associated with the unknown function scope and based on the same alloca.
/// At the same time, propagate the p.addr, p.objId and p.scope.
bool propagateAndConnectNoAliasDecl(Function *F);

/// Find back the 'llvm.noalias.decl' intrinsics in the specified basic blocks
/// and extract their scope. This are candidates for duplication when cloning.
template <typename C>
void identifyNoAliasScopesToClone(ArrayRef<BasicBlock *> BBs,
                                  C &out_NoAliasDeclScopes) {
  for (auto BB : BBs) {
    for (Instruction &I : *BB) {
      if (auto II = dyn_cast<IntrinsicInst>(&I)) {
        if (II->getIntrinsicID() == Intrinsic::noalias_decl) {
          out_NoAliasDeclScopes.push_back(cast<MetadataAsValue>(
              II->getOperand(Intrinsic::NoAliasDeclScopeArg)));
        }
      }
    }
  }
}

/// Duplicate the specified list of noalias decl scopes.
/// The 'Ext' string is added as an extension to the name.
/// Afterwards, the out_ClonedMVScopes contains a mapping of the original MV
/// onto the cloned version.
/// The out_ClonedScopes contains the mapping of the original scope MDNode
/// onto the cloned scope.
void cloneNoAliasScopes(
    ArrayRef<MetadataAsValue *> NoAliasDeclScopes,
    DenseMap<MDNode *, MDNode *> &out_ClonedScopes,
    DenseMap<MetadataAsValue *, MetadataAsValue *> &out_ClonedMVScopes,
    StringRef Ext, LLVMContext &Context);

/// Adapt the metadata for the specified instruction according to the
/// provided mapping. This is normally used after cloning an instruction, when
/// some noalias scopes needed to be cloned.
void adaptNoAliasScopes(
    Instruction *I, DenseMap<MDNode *, MDNode *> &ClonedScopes,
    DenseMap<MetadataAsValue *, MetadataAsValue *> &ClonedMVScopes,
    LLVMContext &Context);

/// Clone the specified noalias decl scopes. Then adapt all instructions in the
/// NewBlocks basicblocks to the cloned versions.
/// 'Ext' will be added to the duplicate scope names
void cloneAndAdaptNoAliasScopes(ArrayRef<MetadataAsValue *> NoAliasDeclScopes,
                                ArrayRef<BasicBlock *> NewBlocks,
                                LLVMContext &Context, StringRef Ext);
} // end namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_NOALIASUTILS_H
