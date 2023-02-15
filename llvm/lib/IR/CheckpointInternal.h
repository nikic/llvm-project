//===- CheckpointInternal.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//

#ifndef LLVM_IR_CHECKPOINTINTERNAL_H
#define LLVM_IR_CHECKPOINTINTERNAL_H

#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Debug.h"

namespace llvm {

class Function;
class BasicBlock;
class Instruction;
class Value;
class MDNode;
class User;
class Use;
class PHINode;
class CallBase;
class CheckpointEngine;
class ValueHandleBase;
class Comdat;
class ReplaceableMetadataImpl;

enum class ChangeID : uint8_t {
  SetMetadataID,
  AddMetadataID,
  EraseMetadataID,
  ChangeMetadataID,
  DeleteMetadataID,
  HandleRAUWMetadataID,
  MetadataUpdateUseMapID,
  MetadataChangeOperandID,
  DeleteObjID,
  ClearMetadataID,
  SetNameID,
  TakeNameID,
  DestroyNameID,
  InsertInstrID,
  RemoveInstrID,
  HandleOperandChangeID,
  AddToConstantUniqueMapID,
  RemoveFromConstantUniqueMapID,
  AddToConstantMapID,
  RemoveFromConstantMapID,
  SetOperandID,
  SetShuffleMaskID,
  SwapUseID,
  SetIncomingBlocksID,
  SetNumUserOperandsID,
  SetSubclassDataID,
  SetSubclassOptionalDataID,
  SetGlobalValueSubClassDataID,
  GlobalValueBitfieldID,
  RemoveBBID,
  MoveBBID,
  SpliceBBID,
  InsertBBID,
  SpliceFnID,
  RemoveFnID,
  CreateValueID,
  DeleteValueID,
  DestroyConstantID,
  CreateValueHandleID,
  DeleteValueHandleID,
  SetFnAttributesID,
  SetCallBaseAttributesID,
  SetGlobalVariableAttributesID,
  SetGlobalVariableInitializerID,
  SetGlobalVariableBitsID,
  RemoveGlobalVariableID,
  DeleteGlobalVariableID,
  RemoveGlobalAliasID,
  DeleteGlobalAliasID,
  InsertGlobalAliasID,
  RemoveGlobalIFuncID,
  DeleteGlobalIFuncID,
  InsertGlobalIFuncID,
  RemoveNamedMDNodeID,
  DeleteNamedMDNodeID,
  InsertNamedMDNodeID,
  SetComdatID,
  DebugLocID,
  ClearInstListID,
};

class ChangeBase {
protected:
  Value *V;
  ChangeID ID;
  bool RevertDeletesValue;
  CheckpointEngine *Parent;

#ifndef NDEBUG
  /// \Returns the unique ID of this object. Used for debugging.
  LLVM_DUMP_METHOD uint32_t getUid() const;
  void dumpCommon(raw_ostream &OS) const;
  void addDump(Value *V);
  std::string getDump(Value *V) const;
#endif // NDEBUG

public:
  ChangeBase(Value *V, ChangeID ID, CheckpointEngine *CE);
  /// Reverts the change.
  virtual void revert() = 0;
  /// If we decide to accept the current state we call this to finalize.
  virtual void apply() = 0;
  ChangeID getID() const { return ID; }
  virtual ~ChangeBase() {}
#ifndef NDEBUG
  virtual void dump(raw_ostream &OS) const = 0;
  LLVM_DUMP_METHOD virtual void dump() const = 0;
#endif // NDEBUG
};

class SetMetadata : public ChangeBase {
  unsigned KindID;
  /// Holds the MD node before we update it, or nullptr.
  MDNode *OrigNode;

public:
  SetMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetMetadataID;
  }
  ~SetMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class AddMetadata : public ChangeBase {
  unsigned KindID;

public:
  AddMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::AddMetadataID;
  }
  ~AddMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class EraseMetadata : public ChangeBase {
  unsigned KindID;
  SmallVector<MDNode *, 1> MDs;

public:
  EraseMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::EraseMetadataID;
  }
  ~EraseMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class ChangeMetadata : public ChangeBase {
  Metadata *OrigMD;

public:
  ChangeMetadata(MetadataAsValue *MAV, Metadata *OrigMD, CheckpointEngine *CE);
  void revert() override;
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::ChangeMetadataID;
  }
  ~ChangeMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteMetadata : public ChangeBase {
  Metadata *MD;

public:
  DeleteMetadata(Metadata *MD, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteMetadataID;
  }
  ~DeleteMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class HandleRAUWMetadata : public ChangeBase {
  Value *To;

public:
  HandleRAUWMetadata(Value *From, Value *To, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::HandleRAUWMetadataID;
  }
  ~HandleRAUWMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class MetadataUpdateUseMap : public ChangeBase {
  ReplaceableMetadataImpl *Def;
  Metadata **MDPtr;
  Metadata *OrigMD;
  uint64_t UseNum;

public:
  MetadataUpdateUseMap(ReplaceableMetadataImpl *Def, Metadata **MDPtr,
                       uint64_t UseNum, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::MetadataUpdateUseMapID;
  }
  ~MetadataUpdateUseMap() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class MetadataChangeOperand : public ChangeBase {
  Metadata *OwnerMD;
  Metadata **MDPtr;
  Metadata *OrigOperand;

public:
  MetadataChangeOperand(Metadata *OwnerMD, Metadata **MDPtr,
                        CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::MetadataChangeOperandID;
  }
  ~MetadataChangeOperand() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename T> class DeleteObj : public ChangeBase {
  T *Ptr;

public:
  DeleteObj(T *Ptr, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteObjID;
  }
  ~DeleteObj() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class ClearMetadata : public ChangeBase {
  SmallVector<std::pair<unsigned, MDNode *>, 1> OrigMetadata;

public:
  ClearMetadata(Value *Val, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::ClearMetadataID;
  }
  ~ClearMetadata() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetName : public ChangeBase {
  std::string OrigName;

public:
  SetName(Value *V, CheckpointEngine *CE);
  void revert() override;
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetNameID;
  }
  ~SetName() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class TakeName : public ChangeBase {
  std::string OrigName;
  Value *FromV;

public:
  TakeName(Value *Val, Value *FromV, CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::TakeNameID;
  }
  ~TakeName() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DestroyName : public ChangeBase {
public:
  DestroyName(Value *Val, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DestroyNameID;
  }
  ~DestroyName() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class InsertInstr : public ChangeBase {
public:
  InsertInstr(Instruction *I, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::InsertInstrID;
  }
  ~InsertInstr() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

/// A Helper class for RemoveInstr and EraseInstr.
class ChkpntInstrUtils {
public:
  /// Helper for inserts \p I after \p PrevInstrOrBB which may be either the
  /// previous instruction or the BB where \p I should be inserted at the top.
  static void insertAfter(Instruction *I, Value *PrevInstrOrBB);
};

class RemoveInstr : public ChangeBase, public ChkpntInstrUtils {
  /// If `PrevInstrOrBB` is an Instruction, then it is the instruction before
  /// the deleted one. If `Before` is a BasicBlock, then the deleted instr was
  /// at the top of `PrevInstrOrBB` BB.
  Value *PrevInstrOrBB;

public:
  RemoveInstr(Instruction *I, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveInstrID;
  }
  ~RemoveInstr() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename ConstantClass>
class HandleOperandChange : public ChangeBase {
  Value *From;
  Value *To;

public:
  HandleOperandChange(ConstantClass *CP, Value *From, Value *To,
                      CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::HandleOperandChangeID;
  }
  ~HandleOperandChange() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
class AddToConstantUniqueMap : public ChangeBase {
  ConstantUniqueMapTy *Map;
  LookupKeyHashedTy Key;

public:
  AddToConstantUniqueMap(ConstantClass *CP, const LookupKeyHashedTy &Key,
                         ConstantUniqueMapTy *Map, CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::AddToConstantUniqueMapID;
  }
  ~AddToConstantUniqueMap() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename ConstantClass, typename ConstantUniqueMapTy>
class RemoveFromConstantUniqueMap : public ChangeBase {
  ConstantUniqueMapTy *Map;

public:
  RemoveFromConstantUniqueMap(ConstantClass *CP, ConstantUniqueMapTy *Map,
                              CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveFromConstantUniqueMapID;
  }
  ~RemoveFromConstantUniqueMap() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename MapClass> class AddToConstantMap : public ChangeBase {
  MapClass *Map;
  typename MapClass::key_type Key;

public:
  AddToConstantMap(typename MapClass::key_type Key,
                   typename MapClass::mapped_type Val, MapClass *Map,
                   CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::AddToConstantMapID;
  }
  ~AddToConstantMap() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

template <typename MapClass> class RemoveFromConstantMap : public ChangeBase {
  MapClass *Map;
  using KeyT = typename MapClass::key_type;
  KeyT Key;

public:
  RemoveFromConstantMap(typename MapClass::key_type Key, MapClass *Map,
                        CheckpointEngine *CE);
  void revert() override;
  void apply() override{};
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveFromConstantMapID;
  }
  ~RemoveFromConstantMap() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetFnAttributes : public ChangeBase {
  AttributeList OrigAttrs;

public:
  SetFnAttributes(Function *F, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetFnAttributesID;
  }
  ~SetFnAttributes() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetCallBaseAttributes : public ChangeBase {
  AttributeList OrigAttrs;

public:
  SetCallBaseAttributes(CallBase *C, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetCallBaseAttributesID;
  }
  ~SetCallBaseAttributes() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetGlobalVariableAttributes : public ChangeBase {
  AttributeSet OrigAttrSet;

public:
  SetGlobalVariableAttributes(GlobalVariable *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetGlobalVariableAttributesID;
  }
  ~SetGlobalVariableAttributes() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetGlobalVariableInitializer : public ChangeBase {
  Constant *OrigInitVal;

public:
  SetGlobalVariableInitializer(GlobalVariable *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetGlobalVariableInitializerID;
  }
  ~SetGlobalVariableInitializer() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetGlobalVariableBits : public ChangeBase {
  struct BitsData {
    uint8_t isConstantGlobal : 1;
    uint8_t isExternallyInitializedConstant : 1;
  };
  BitsData Bits;

public:
  SetGlobalVariableBits(GlobalVariable *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetGlobalVariableBitsID;
  }
  ~SetGlobalVariableBits() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteGlobalVariable : public ChangeBase {
public:
  DeleteGlobalVariable(GlobalVariable *GV, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteGlobalVariableID;
  }
  ~DeleteGlobalVariable() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveGlobalVariable : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalVariable *, Module *> PrevGVOrModule;

public:
  RemoveGlobalVariable(GlobalVariable *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveGlobalVariableID;
  }
  ~RemoveGlobalVariable() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveGlobalAlias : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalAlias *, Module *> PrevGVOrModule;

public:
  RemoveGlobalAlias(GlobalAlias *GA, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveGlobalAliasID;
  }
  ~RemoveGlobalAlias() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteGlobalAlias : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalAlias *, Module *> PrevGVOrModule;

public:
  DeleteGlobalAlias(GlobalAlias *GV, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteGlobalAliasID;
  }
  ~DeleteGlobalAlias() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class InsertGlobalAlias : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalAlias *, Module *> PrevGVOrModule;

public:
  InsertGlobalAlias(GlobalAlias *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::InsertGlobalAliasID;
  }
  ~InsertGlobalAlias() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveGlobalIFunc : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalIFunc *, Module *> PrevGVOrModule;

public:
  RemoveGlobalIFunc(GlobalIFunc *GIF, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveGlobalIFuncID;
  }
  ~RemoveGlobalIFunc() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteGlobalIFunc : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalIFunc *, Module *> PrevGVOrModule;

public:
  DeleteGlobalIFunc(GlobalIFunc *GIF, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteGlobalIFuncID;
  }
  ~DeleteGlobalIFunc() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class InsertGlobalIFunc : public ChangeBase {
  /// The previous global variable or the module if at the top.
  std::variant<GlobalIFunc *, Module *> PrevGVOrModule;

public:
  InsertGlobalIFunc(GlobalIFunc *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::InsertGlobalIFuncID;
  }
  ~InsertGlobalIFunc() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveNamedMDNode : public ChangeBase {
  NamedMDNode *RemovedNode;
  /// The previous global variable or the module if at the top.
  std::variant<NamedMDNode *, Module *> PrevGVOrModule;

public:
  RemoveNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveNamedMDNodeID;
  }
  ~RemoveNamedMDNode() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteNamedMDNode : public ChangeBase {
  NamedMDNode *DeletedNode;
  /// The previous global variable or the module if at the top.
  std::variant<NamedMDNode *, Module *> PrevGVOrModule;

public:
  DeleteNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteNamedMDNodeID;
  }
  ~DeleteNamedMDNode() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class InsertNamedMDNode : public ChangeBase {
  NamedMDNode *InsertedNode;
  /// The previous global variable or the module if at the top.
  std::variant<NamedMDNode *, Module *> PrevGVOrModule;

public:
  InsertNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::InsertNamedMDNodeID;
  }
  ~InsertNamedMDNode() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetComdat : public ChangeBase {
  Comdat *OrigComdat;

public:
  SetComdat(GlobalObject *GO, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetComdatID;
  }
  ~SetComdat() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetDebugLoc : public ChangeBase {
  DebugLoc OriginalLoc;

public:
  SetDebugLoc(Instruction *I, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DebugLocID;
  }
  ~SetDebugLoc() {}
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetOperand : public ChangeBase {
  uint32_t OpIdx;
  Value *Op;

public:
  SetOperand(User *U, uint32_t OpIdx, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetOperandID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetShuffleMask : public ChangeBase {
  SmallVector<int> OrigMask;

public:
  SetShuffleMask(ShuffleVectorInst *Shuffle, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetShuffleMaskID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

// TODO: We don't use ChangeBase::V. Inherit from a value-less base class.
class SwapUse : public ChangeBase {
  Use *U1;
  Use *U2;

public:
  SwapUse(Use *U1, Use *U2, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SwapUseID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetIncomingBlocks : public ChangeBase {
  uint32_t Idx;
  SmallVector<BasicBlock *> OrigBBs;

public:
  SetIncomingBlocks(PHINode *Phi, uint32_t Idx, uint32_t NumBlocks,
                    CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetIncomingBlocksID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetNumUserOperands : public ChangeBase {
  uint32_t NumOps;

public:
  SetNumUserOperands(User *U, uint32_t NumOps, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetNumUserOperandsID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetSubclassData : public ChangeBase {
  uint16_t OrigData;

public:
  SetSubclassData(Value *Val, uint16_t OrigData, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetSubclassDataID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetSubclassOptionalData : public ChangeBase {
  uint8_t OrigData;

public:
  SetSubclassOptionalData(Value *Val, uint16_t OrigData, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetSubclassOptionalDataID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SetGlobalValueSubClassData : public ChangeBase {
  uint16_t OrigData;

public:
  SetGlobalValueSubClassData(Value *Val, uint16_t OrigData, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SetGlobalValueSubClassDataID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class GlobalValueBitfield : public ChangeBase {
  uint32_t OrigBitfield;

public:
  GlobalValueBitfield(GlobalValue *GV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::GlobalValueBitfieldID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveBB : public ChangeBase {
  Function *F;
  // If null, then we are at the end of the function.
  BasicBlock *NextBB;

public:
  RemoveBB(BasicBlock *BB, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveBBID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class MoveBB : public ChangeBase {
  // If PrevBBOrFn is a BasicBlock it is the previous BasicBlock in the ilist.
  // If it is a Function, then the BB should be placed at the top of the Fn.
  Value *PrevBBOrFn;

public:
  MoveBB(BasicBlock *BB, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::MoveBBID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SpliceBB : public ChangeBase {
  Instruction *FirstI;
  Instruction *LastI;

public:
  SpliceBB(Value *OrigInstrOrBB, Instruction *FirstI, Instruction *LastI,
           CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SpliceBBID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class InsertBB : public ChangeBase {
public:
  InsertBB(BasicBlock *BB, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::InsertBBID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class SpliceFn : public ChangeBase {
  BasicBlock *FirstBB;
  BasicBlock *LastBB;

public:
  SpliceFn(Value *OrigBBOrFn, BasicBlock *FirstBB, BasicBlock *LastBB,
           CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::SpliceFnID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class RemoveFn : public ChangeBase {
  Module *M;
  // If null, then we are at the end of the Function list.
  Function *NextFn;

public:
  RemoveFn(Function *Fn, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::RemoveFnID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class CreateValue : public ChangeBase {
public:
  CreateValue(Value *NewV, CheckpointEngine *CE);
  void revert() override;
  void apply() override {}
  static bool classof(const ChangeBase *Other) {
    return Other->getID() == ChangeID::CreateValueID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteValue : public ChangeBase {
public:
  DeleteValue(Value *DelV, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DeleteValueID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DestroyConstant : public ChangeBase {
public:
  DestroyConstant(Constant *C, CheckpointEngine *CE);
  void revert() override {}
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::DestroyConstantID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

// TODO: This needs to inherit from a base class without a Value member.
class CreateValueHandle : public ChangeBase {
  ValueHandleBase *VH;

public:
  CreateValueHandle(ValueHandleBase *VH, CheckpointEngine *CE);
  ValueHandleBase &getVH() const { return *VH; }
  void revert() override {}
  void apply() override {}
  static bool classof(const ChangeBase *Other) {
    return Other->getID() == ChangeID::CreateValueHandleID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

class DeleteValueHandle : public ChangeBase {
  ValueHandleBase *VH;

public:
  DeleteValueHandle(ValueHandleBase *VH, CheckpointEngine *CE);
  ValueHandleBase &getVH() const { return *VH; }
  void revert() override {}
  void apply() override {}
  static bool classof(const ChangeBase *Other) {
    return Other->getID() == ChangeID::DeleteValueHandleID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

/// When we clear a BB's instruction list.
class ClearInstList : public ChangeBase {
  /// We transfer the instructions to this temporary BB.
  std::unique_ptr<BasicBlock> TmpBB;

public:
  ClearInstList(BasicBlock *BB, CheckpointEngine *CE);
  void revert() override;
  void apply() override;
  static bool classof(const ChangeBase &Other) {
    return Other.getID() == ChangeID::ClearInstListID;
  }
#ifndef NDEBUG
  void dump(raw_ostream &OS) const override;
  LLVM_DUMP_METHOD void dump() const override;
#endif // NDEBUG
};

} // namespace llvm

#endif // LLVM_IR_CHECKPOINTINTERNAL_H
