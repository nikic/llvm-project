//===- CheckpointEngine.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Note: This header file should not be included by chekcpointing clients.
// Please include IR/Checkpoint.h instead.
//
// This should be used only by the IR classes that need to notify the
// checkpointing engine about modifications to the IR.
//

#ifndef LLVM_IR_CHECKPOINTENGINE_H
#define LLVM_IR_CHECKPOINTENGINE_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include <memory>
#include <regex>

namespace llvm {

#ifndef NDEBUG
class Module;
/// Helper class that saves the textual representation of the IR upon
/// construction and compares against it when `expectNoChange()` is called.
class IRChecker {
  Module *M = nullptr;
  bool SkipPreds;
  std::string OriginalIR;
  std::string PredsRegexStr = "; preds = .*\n";
  std::regex PredsRegex;
  /// \Returns the IR dump in a string.
  std::string dumpIR() const;
  /// Prints a simple line diff between \p OrigIR and \p CurrIR.
  static void showDiff(const std::string &OrigIR, const std::string &CurrIR);

public:
  /// If SkipPreds is true we remove the "; preds = .*" string from the dumps.
  /// This helps avoids false positives since Checkpoint cannot currently
  /// preserve the order of users.
  IRChecker(bool SkipPreds = true);
  IRChecker(Module &M, bool SkipPreds = true);
  void save(Module &M);
  void save();
  /// \Returns the dump of the original IR.
  const std::string &origIR() const { return OriginalIR; }
  /// \Returns the dump of the current IR.
  std::string currIR() const { return dumpIR(); }
  /// Crashes if there is a difference between the original and current IR.
  void expectNoDiff() const;
};
#endif // NDEBUG

class ChangeBase;
class Use;
class User;
class Value;
class Instruction;
class BasicBlock;
class Function;
class PHINode;
class Attribute;
class GlobalVariable;
class CallBase;
class ShuffleVectorInst;
class GlobalValue;
class GlobalAlias;
class GlobalIFunc;
class MetadataAsValue;
class ValueAsMetadata;
class Metadata;
class GlobalObject;
class ConstantArray;
class Constant;
class NamedMDNode;
class ReplaceableMetadataImpl;

#ifndef NDEBUG
/// Holds the string representation of some of the values that become malformed
/// as the IR gets transformed. This is useful for debugging checkpointing
/// internals.
class ValueDump {
  DenseMap<Value *, std::string> Map;

public:
  void add(Value *V);
  std::string get(Value *V) const;
};
#endif // NDEBUG

class CheckpointEngine;

/// A simple guard class that deactivates checkpointing on construction and
/// reactivates it on destruction.
class CheckpointGuard {
  CheckpointEngine *Chkpnt;
  bool LastState;
  friend class CheckpointEngine;
  /// Private by design. Use CheckpointEngine::disable() to get a guard.
  CheckpointGuard(bool NewState, CheckpointEngine *Chkpnt);

public:
  ~CheckpointGuard();
};

/// This is the main class for the checkpointing internals. This is where
/// the changes get recorded.
class CheckpointEngine {
  friend class CheckpointGuard; // Needs access to `Active`.
  /// This is true while checkpointing is active.
  bool Active = false;
  /// This is true during rollback.
  bool InRollback = false;
#ifndef NDEBUG
  /// Controls whether we are running the verifier in rollback().
  bool RunVerifier;
  /// The verifier.
  IRChecker IRChecker;
#endif // NDEBUG
  /// A limit to the number of changes we will record. This is set by
  /// CheckpointHandle::save(<NUM>) and is useful for debugging. We will get a
  /// crash if we go over this limit.
  uint32_t MaxNumChanges = 0;
  /// The sequence of changes applied to the IR in the order they take place.
  SmallVector<std::unique_ptr<ChangeBase>> Changes;

#ifndef NDEBUG
  // To access Active, ValDump, ChangeUids in a debug build.
  friend class ChangeBase;
  ValueDump ValDump;
  /// Unique ID for each change object, for debugging.
  DenseMap<ChangeBase *, uint32_t> ChangeUids;
#endif // NDEBUG

  friend class Instruction;
  friend class Value;
  friend class BasicBlock;
  friend class User; // setNumUserOperands()
  inline bool isActive() const { return Active; }
  inline bool inRollback() const { return InRollback; }
  friend class Checkpoint;
  friend class PHINode;  // setIncomingBlock()
  friend class FnAttributeList;
  friend class GlobalVariableAttributeSet;
  friend class CallBaseAttributeList;
  friend class Function;  // insert()
  friend class CheckpointSavePass;    // Calls save()
  friend class CheckpointAcceptPass;  // Calls accept()
  friend class CheckpointRollbackPass; // Calls rollback()
  friend class GEPOperator;            // isActive()
  friend class OverflowingBinaryOperator;  // isActive()
  friend class PossiblyExactOperator;      // isActive()
  friend class FPMathOperator;             // isActive()
  friend class Use;                        // isActive()
  friend class ShuffleVectorInst;          // isActive()
  friend class GlobalValue;                // isActive()
  friend class MetadataAsValue;            // isActive()
  friend class ValueAsMetadata;            // isActive()
  friend class MDNode;                     // isActive()
  friend class GVBitfields;                // isActive()
  friend class GlobalVariable;             // isActive()
  friend class Module;                     // isActive()
  friend class GlobalAlias;                // isActive()
  friend class GlobalIFunc;                // isActive()
  friend class GlobalObject;               // isActive()
  friend class ConstantArray;              // isActive()
  friend class ConstantStruct;             // isActive()
  friend class ConstantVector;             // isActive()
  friend class ConstantExpr;               // isActive()
  template <typename T> friend class ConstantUniqueMap; // isActive()
  friend class Constant;                                // isActive()
  friend class NoCFIValue;                              // isActive()
  friend class DSOLocalEquivalent;                      // isActive()
  friend class BlockAddress;                            // isActive()
  friend class ReplaceableMetadataImpl;                 // isActive()

  /// \Returns true if there are no changes in the queue.
  bool empty() const { return Changes.empty(); };
  /// \Returns the number of changes in the queue.
  uint32_t size() const { return Changes.size(); }
  /// Clears the state.
  void clear();
  /// To be called when \p V is about to get its metadata set.
  void setMetadata(Value *V, unsigned KindID);
  void addMetadata(Value *V, unsigned KindID);
  void eraseMetadata(Value *V, unsigned KindID);
  void clearMetadata(Value *V);
  /// Called by MetadataAsValue::handleChangedMetadata().
  void changeMetadata(MetadataAsValue *MAV, Metadata *OrigMD);
  void deleteMetadata(Metadata *MD);
  void handleRAUWMetadata(Value *From, Value *To);
  // TODO: This is broken.
  void metadataUpdateUseMap(ReplaceableMetadataImpl *Def, Metadata **OrigMDPtr,
                            uint64_t UseNum);
  void metadataChangeOperand(Metadata *OwnerMD, Metadata **MDPtr);
  template <typename T> void deleteObj(T *Ptr);
  /// To be called when \p V is about to get its name updated.
  void setName(Value *V);
  /// Called by V->takeName(FromV).
  void takeName(Value *V, Value *FromV);
  /// Gets called by ~Value() before destroyValueName().
  void destroyName(Value *V);
  /// Track a new instruction that gets inserted into a BB.
  void insertInstr(Instruction *I);
  /// Track the removal of \p I.
  void removeInstr(Instruction *I);
  template <typename ConstantClass, typename ConstantUniqueMapTy,
            typename LookupKeyHashedTy>
  void addToConstantUniqueMap(ConstantClass *C, const LookupKeyHashedTy &Key,
                              ConstantUniqueMapTy *Map);
  template <typename ConstantClass, typename ConstantUniqueMapTy>
  void removeFromConstantUniqueMap(ConstantClass *C,
                                   ConstantUniqueMapTy *Map);

  /// For constants that don't use a ConstantUniqueMap.
  template <typename MapClass>
  void addToConstantMap(typename MapClass::key_type Key,
                        typename MapClass::mapped_type Val, MapClass *Map);
  template <typename MapClass>
  void removeFromConstantMap(typename MapClass::key_type Key, MapClass *Map);
  /// Take note of the \p OpIdx'th operand of \p U.
  void setOperand(User *U, uint32_t OpIdx);
  /// Called by ShuffleVectorInst::setShuffleMask()
  void setShuffleMask(ShuffleVectorInst *Shuffle);
  /// Called by Use::swap(Use &)
  void swapUse(Use *U1, Use *U2);
  /// To be called when we are about to set \p NumBlocks of \p Phi's incoming
  /// blocks, starting at \p Idx.
  void setIncomingBlocks(PHINode *Phi, uint32_t Idx, uint32_t NumBlocks = 1u);
  /// To be called just before NumUserOperands changes.
  void setNumUserOperands(User *U, uint32_t NumUserOperands);
  /// To be called in Instruction::setSubclassData().
  void setSubclassData(Value *V, uint16_t Data);
  void setSubclassOptionalData(Value *V, uint16_t Data);
  void setGlobalValueSubClassData(GlobalValue *GV, uint16_t Data);
  void setGlobalValueBitfield(GlobalValue *GV);
  /// Gets called just before we set \p C call-site's attributes.
  void setCallBaseAttributes(CallBase *C);
  /// Gets called just before we set F's attributes.
  void setFnAttributes(Function *F);
  /// Gets called just before we set GV's attributes.
  void setGlobalVariableAttributes(GlobalVariable *GV);
  void setGlobalVariableInitializer(GlobalVariable *GV);
  /// Called when bits like GlobalVariable::isConstantGlobal.
  void setGlobalVariableBits(GlobalVariable *GV);
  void removeGlobalVariable(GlobalVariable *GV);
  void deleteGlobalVariable(GlobalVariable *GV);

  void removeGlobalAlias(GlobalAlias *GA);
  void deleteGlobalAlias(GlobalAlias *GA);
  void insertGlobalAlias(GlobalAlias *GA);

  void removeGlobalIFunc(GlobalIFunc *GIF);
  void deleteGlobalIFunc(GlobalIFunc *GIF);
  void insertGlobalIFunc(GlobalIFunc *GIF);

  void removeNamedMDNode(NamedMDNode *MDNode);
  void deleteNamedMDNode(NamedMDNode *MDNode);
  void insertNamedMDNode(NamedMDNode *MDNode);

  void setComdat(GlobalObject *GO);
  /// To be called right after \p I got its DebugLoc updated.
  void setDebugLoc(Instruction *I);
  /// To be called when \p BB is removed from parent.
  void removeBB(BasicBlock *BB);
  /// To be called when \p BB is moved by moveBefore/After.
  void moveBB(BasicBlock *BB);
  /// To be called after the chain of instructions \p FirstI to (including) \p
  /// LastI got transferred from \p OrigInstrOrBB to their current location.
  void spliceBB(Value *OrigInstrOrBB, Instruction *FirstI, Instruction *LastI);
  /// To be called when a \p BB gets inserted into a function.
  void insertBB(BasicBlock *B);
  /// Called after the chain of BBs \p FirstBB to (including) \p LastBB get
  /// transferred from \p OrigBBOrFn to their current location.
  void spliceFn(Value *OrigBBOrFn, BasicBlock *FirstBB, BasicBlock *LastBB);
  /// Gets called by Funciton::eraseFromParent().
  void removeFn(Function *F);
  /// Gets called in Value constructor.
  void createValue(Value *NewV);
  /// Gets called in Value destructor.
  void deleteValue(Value *DelV);
  /// Called by Constant::destroyConstant().
  void destroyConstant(Constant *C);
  /// Apply any changes. This is also called by the destructor and before
  /// starting a new checkpoint with save().
  void accept();
  /// Gets called by the destructor of the BB, before its instruction list gets
  /// cleared.
  void clearInstList(BasicBlock *BB);

  // Main API functions. These are called by `Checkpoint`.

  /// Start tracking IR changes from this point on. Rollback will restore the
  /// state of the IR to this point.
  ///
  /// \p RunVerifier runs *very* expensive checks that compare the modules state
  /// between save() and rollback().
  ///
  /// \p MaxNumOfTrackedChanges is used for debugging and will crash if we
  /// record more changes that this number.
  void startTracking(bool RunVerifier, uint32_t MaxNumOfTrackedChanges);
  /// Restores the instructions to the state before the checkpoint.
  void rollback();
  /// Deactivate checkpointing as long as the returned guard is in-scope.
  CheckpointGuard disable();
  // Need to call disable():
  friend class ClearInstList;
  friend class SetName;
  friend class TakeName;
  friend class SetShuffleMask;
  friend class SetMetadata;
  friend class AddMetadata;
  friend class EraseMetadata;
  friend class ClearMetadata;
  friend class Function; // Function::eraseFromParent()
  friend class SetGlobalValueSubClassData;
  friend class RAUWMetadata;  // revert();
  friend class RAUWMetadataMD;
  friend class ChangeMetadata;
  friend class DeleteGlobalAlias;
  friend class DeleteGlobalIFunc;
  friend class DeleteNamedMDNode;

#ifndef NDEBUG
  void dump(raw_ostream &OS) const;
  LLVM_DUMP_METHOD void dump() const;
#endif // NDEBUG

public:
  CheckpointEngine();
  ~CheckpointEngine();
  /// \Returns the previous instruction of \p I in the instruction list, or the
  /// parent BB if at the top.
  static Value *getPrevInstrOrParent(Instruction *I);
  /// \Returns the previous BasicBlock of \p BB in the function's BasicBlock
  /// list, or the parent Function if at the top.
  static Value *getPrevBBOrParent(BasicBlock *BB);
};
} // namespace llvm

#endif // LLVM_IR_CHECKPOINTENGINE_H
