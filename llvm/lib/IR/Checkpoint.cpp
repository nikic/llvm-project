//===- Checkpoint.cpp -----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Checkpoint.h"
#include "llvm/IR/CheckpointEngine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

Checkpoint::~Checkpoint() {
  assert(ChkpntEngine.empty() &&
         "Missing call to Checkpoint::accept() or Checkpoint::rollback()");
}

void Checkpoint::save(uint32_t MaxNumOfTrackedChanges) {
  ChkpntEngine.startTracking(RunVerifier, MaxNumOfTrackedChanges);
  assert(ChkpntEngine.Active && "Save() should start tracking");
}

void Checkpoint::rollback() {
  ChkpntEngine.rollback();
  assert(!ChkpntEngine.Active && "We should stop tracking after rollback()");
}

void Checkpoint::accept() {
  ChkpntEngine.accept();
  assert(!ChkpntEngine.Active && "We should stop tracking after accept()");
}

bool Checkpoint::empty() const { return ChkpntEngine.empty(); }

#ifndef NDEBUG
uint32_t Checkpoint::size() const { return ChkpntEngine.size(); }

void Checkpoint::dump(raw_ostream &OS) const { ChkpntEngine.dump(OS); }

void Checkpoint::dump() const { dump(dbgs()); }
#endif // NDEBUG
