//===- Checkpoint.h ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class provides simple lightweight checkpointing for the IR with save()
// and rollback(). Rollback reverts the state of the IR to the state when save()
// was called.
//
// How to use
// ----------
// - Get a checkpoint handle using LLVMContext::getCheckpointHandle():
//     auto Chkpnt = getContext().getCheckpointHandle();
// - Save the IR's state using Chkpnt.save(). This starts tracking of IR changes
// - Modify the IR in any way (e.g. remove an instruction).
// - Restore the original state of the IR using:
//     Chkpnt.rollback();
//   Or accept the current state using:
//     Chkpnt.accept();
// - Don't let the handle go out of scope without calling accept() or rollback()
//
// What gets rolled-back
// ---------------------
//  o The state of the LLVM IR (with a few exceptions see below).
//    Dumping the IR module  after a Chkpnt.rollback() will give you the exact
//    same IR as at Chkpnt.save().
//    This includes Instructions, BasicBlocks, Functions, Metadata etc.
//
// What does not get rolled-back
// -----------------------------
//  o The exact order of users is not currently maintained.
//  o Analyses cannot be rolled back automatically for now.
//  o Creation of Constants is not currently being reverted.
//  o User-defined structures are not obviously not being tracked.
//    - This includes ValueHandles. It is the user's responsibility to
//      cleanup any outstanding ValueHandle objects if needed before a rollback,
//      otherwise they may experience strange behavior due to the ValueHandles
//      taking actions while the the tracked IR values are getting rolled-back.
//
// How it works
// ------------
// Checkpointing works by tracking of all changes made to the IR after the first
// call to CheckpointHandle::save(). Checkpoint::rollback() reverts all changes
// in reverse order, bringing the IR back to its original state.
//
// Please refer to `llvm/lib/IR/CheckpointInternal.h` for the implementation.
//

#ifndef LLVM_IR_CHECKPOINT_H
#define LLVM_IR_CHECKPOINT_H

#include "llvm/Support/Compiler.h"
#include <cstdint>

namespace llvm {

class CheckpointEngine;
class raw_ostream;

class Checkpoint {
  CheckpointEngine &ChkpntEngine;
  bool RunVerifier;
  /// No copies allowed because going out of scope will accept().
  Checkpoint(Checkpoint &) = delete;
  void operator=(const Checkpoint &) = delete;

public:
  /// If \p RunVerifier is true we run expensive checks to compare the modules
  /// state between save() and rollback(). These are used as a sanity check for
  /// checkpointing itself. The checks only run in a debug build, but even so
  /// they are very expensive, so please only use it in tests or during
  /// prototyping.
  Checkpoint(CheckpointEngine &ChkpntEngine, bool RunVerifier = false)
      : ChkpntEngine(ChkpntEngine), RunVerifier(RunVerifier) {}
  ~Checkpoint();
  /// Activates checkpointing and starts tracking changes made to the IR. When
  /// rollback() is called the IR state is reverted to the state of this point.
  /// NOTE: save() is fast, but any change done to the IR is slower than usual
  /// because we track the changes. So it is important to accept() or rollback()
  /// as soon as possible.
  ///
  /// \p MaxNumOfTrackedChanges is used for debugging to help diagnose cases
  /// were the user forgets to apply() or rollback(). It will cause a crash if
  /// we record more changes that this number.
  void save(uint32_t MaxNumOfTrackedChanges = 4096);
  /// Revert the state of the IR to the point when save() was called, and stops
  /// tracking.
  void rollback();
  /// Accept the changes and stops tracking of changes. This performs any
  /// outstanding cleanup actions.
  void accept();

  /// \Returns true if there are no entries to rollback at this point.
  bool empty() const;
#ifndef NDEBUG
  /// \Returns the number of entries.
  uint32_t size() const;
  /// Debug printers.
  void dump(raw_ostream &OS) const;
  LLVM_DUMP_METHOD void dump() const;
#endif // NDEBUG
};
} // namespace llvm
#endif // LLVM_IR_CHECKPOINT_H
