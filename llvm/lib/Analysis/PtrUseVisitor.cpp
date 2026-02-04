//===- PtrUseVisitor.cpp - InstVisitors over a pointers uses --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Implementation of the pointer use visitors.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/PtrUseVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

void detail::PtrUseVisitorBase::enqueueUsers(Value &I) {
  for (Use &U : I.uses()) {
    bool OffsetKnown = IsOffsetKnown;
    APInt OffsetCopy = Offset;

    if (OffsetKnown) {
      // If we're about to visit a PHI that's already in our path,
      // we hit a possibly-infinite cycle. If it had the same value as before,
      // then we are at the fixed point. Otherwise, widen this offset to
      // unknown. This is the only way to infinitely cycle unless we enter dead
      // code, where a value (GEP) could directly depend on itself.
      if (auto *PN = dyn_cast<PHINode>(U.getUser())) {
        auto It = PHIsInPath.find(PN);
        if (It != PHIsInPath.end()) {
          if (It->second == OffsetCopy)
            // Same offset as when we first encountered this PHI, skip
            continue;
          // Different offset, mark as unknown
          OffsetKnown = false;
          OffsetCopy = APInt();
        }
      }
    }

    UseToVisit NewU = {{&U, OffsetKnown}, std::move(OffsetCopy), PHIsInPath};
    UseWithOffset Key = {NewU.UseAndIsOffsetKnown, NewU.Offset};
    if (!VisitedUses.insert(Key).second)
      continue;

    Worklist.push_back(std::move(NewU));
  }
}

bool detail::PtrUseVisitorBase::adjustOffsetForGEP(GetElementPtrInst &GEPI) {
  if (!IsOffsetKnown)
    return false;

  APInt TmpOffset(DL.getIndexTypeSizeInBits(GEPI.getType()), 0);
  if (GEPI.accumulateConstantOffset(DL, TmpOffset)) {
    Offset += TmpOffset.sextOrTrunc(Offset.getBitWidth());
    return true;
  }

  return false;
}
