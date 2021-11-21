//===-- IR/Statepoint.cpp -- gc.statepoint utilities ---  -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains some utility functions to help recognize gc.statepoint
// intrinsics.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Statepoint.h"

#include "llvm/IR/Function.h"

using namespace llvm;

bool llvm::isStatepointDirectiveAttr(Attribute Attr) {
  return Attr.hasAttribute(StatepointIdAttr) ||
         Attr.hasAttribute(StatepointNumPatchBytesAttr);
}

StatepointDirectives
llvm::parseStatepointDirectivesFromAttrs(AttributeList AS) {
  StatepointDirectives Result;

  Attribute AttrID = AS.getFnAttr(StatepointIdAttr);
  uint64_t StatepointID;
  if (AttrID.isStringAttribute())
    if (!AttrID.getValueAsString().getAsInteger(10, StatepointID))
      Result.StatepointID = StatepointID;

  uint32_t NumPatchBytes;
  Attribute AttrNumPatchBytes = AS.getFnAttr(StatepointNumPatchBytesAttr);
  if (AttrNumPatchBytes.isStringAttribute())
    if (!AttrNumPatchBytes.getValueAsString().getAsInteger(10, NumPatchBytes))
      Result.NumPatchBytes = NumPatchBytes;

  return Result;
}
