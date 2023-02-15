//===- CheckpointInternal.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CheckpointInternal.h"
#include "LLVMContextImpl.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Sequence.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Comdat.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/User.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

#ifndef NDEBUG
#if defined(__linux__)
#include <sys/resource.h>
#endif
#endif

using namespace llvm;
using namespace std;

#ifndef NDEBUG
/// \Returns true if \p Ptr is a pointer in the process stack. This is for
/// debugging and catching errors. Checkpointing does not fully support
/// operating on objects in the stack as the memory associated to such objects
/// may have been overwritten by other data by the time rollback() is called.
static bool isStackAddr(void *Ptr) {
#if defined(__linux__)
  char AStackVariable;
  static int64_t StackSize;
  if (StackSize == 0) {
    rlimit RLimit;
    getrlimit(RLIMIT_STACK, &RLimit);
    StackSize = RLimit.rlim_cur;
  }
  return std::abs((char *)Ptr - &AStackVariable) < StackSize;
#else
  return false
#endif // __linux__
}
#endif // NDEBUG

ChangeBase::ChangeBase(Value *V, ChangeID ID, CheckpointEngine *CE)
    : V(V), ID(ID), Parent(CE) {
#ifndef NDEBUG
  Parent->ChangeUids[this] = Parent->ChangeUids.size() + 1;
  assert(!isStackAddr(V) && "Objects in the stack are not supported!");
  assert(Parent->Active && "Need to call save() first");
  assert(Parent->Changes.size() + 1 < Parent->MaxNumChanges &&
         "Tracking too many changes!");
#endif
}

#ifndef NDEBUG
uint32_t ChangeBase::getUid() const { return Parent->ChangeUids.lookup(this); }
void ChangeBase::dumpCommon(raw_ostream &OS) const { OS << getUid() << ". "; }
void ChangeBase::addDump(Value *V) { Parent->ValDump.add(V); }

std::string ChangeBase::getDump(Value *V) const {
  bool SvActive = Parent->Active;
  // Lazy function arguments get created when we dump, so deactivate.
  Parent->Active = false;
  auto Ret = Parent->ValDump.get(V);
  Parent->Active = SvActive;
  return Ret;
}
#endif

SetMetadata::SetMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::SetMetadataID, CE), KindID(KindID) {
  OrigNode = Val->getMetadata(KindID);
}

void SetMetadata::revert() {
  auto DisableGuard = Parent->disable();
  V->setMetadata(KindID, OrigNode);
}

#ifndef NDEBUG
void SetMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetMetadata:" << *V << "   KindID=" << KindID << " OrigNode=";
  if (OrigNode != nullptr)
    OS << *OrigNode;
  else
    OS << "NULL";
  OS << "\n";
}

void SetMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

AddMetadata::AddMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::AddMetadataID, CE), KindID(KindID) {}

void AddMetadata::revert() {
  auto DisableGuard = Parent->disable();
  V->eraseMetadata(KindID);
}

#ifndef NDEBUG
void AddMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "AddMetadata:" << *V << "   KindID=" << KindID << "\n";
}

void AddMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

EraseMetadata::EraseMetadata(Value *Val, unsigned KindID, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::EraseMetadataID, CE), KindID(KindID) {
  Val->getMetadata(KindID, MDs);
}

void EraseMetadata::revert() {
  auto DisableGuard = Parent->disable();
  for (MDNode *MD : MDs)
    V->addMetadata(KindID, *MD);
}

#ifndef NDEBUG
void EraseMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "EraseMetadata:" << *V << "   KindID=" << KindID << "\n";
}

void EraseMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

ChangeMetadata::ChangeMetadata(MetadataAsValue *MAV, Metadata *OrigMD,
                               CheckpointEngine *CE)
    : ChangeBase(MAV, ChangeID::ChangeMetadataID, CE), OrigMD(OrigMD) {}

void ChangeMetadata::revert() {
  cast<MetadataAsValue>(V)->handleChangedMetadata(OrigMD);
}

void ChangeMetadata::apply() { delete cast<MetadataAsValue>(V); }

#ifndef NDEBUG
void ChangeMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "ChangeMetadata:" << getDump(V) << "\n";
}

void ChangeMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteMetadata::DeleteMetadata(Metadata *MD, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::DeleteMetadataID, CE), MD(MD) {}

void DeleteMetadata::apply() { delete MD; }

#ifndef NDEBUG
void DeleteMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteMetadata\n";
}

void DeleteMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

HandleRAUWMetadata::HandleRAUWMetadata(Value *From, Value *To,
                                       CheckpointEngine *CE)
    : ChangeBase(From, ChangeID::HandleRAUWMetadataID, CE), To(To) {}

void HandleRAUWMetadata::revert() {
  Value *From = V;
  ValueAsMetadata::handleRAUW(To, From, /*DontDelete=*/true);
}

#ifndef NDEBUG
void HandleRAUWMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "HandleRAUWMetadata\n";
}

void HandleRAUWMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

MetadataUpdateUseMap::MetadataUpdateUseMap(ReplaceableMetadataImpl *Def,
                                           Metadata **MDPtr, uint64_t UseNum,
                                           CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::MetadataUpdateUseMapID, CE), Def(Def),
      MDPtr(MDPtr), OrigMD(*MDPtr), UseNum(UseNum) {}

void MetadataUpdateUseMap::revert() {
  *MDPtr = OrigMD;
  auto &ValuePair = Def->UseMap[MDPtr];
  ValuePair = {OrigMD, UseNum};
}

#ifndef NDEBUG
void MetadataUpdateUseMap::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "MetadataUpdateUseMap\n";
}

void MetadataUpdateUseMap::dump() const { dump(dbgs()); }
#endif // NDEBUG

MetadataChangeOperand::MetadataChangeOperand(Metadata *OwnerMD,
                                             Metadata **MDPtr,
                                             CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::MetadataChangeOperandID, CE),
      OwnerMD(OwnerMD), MDPtr(MDPtr), OrigOperand(*MDPtr) {}

void MetadataChangeOperand::revert() {
  switch (OwnerMD->getMetadataID()) {
#define HANDLE_METADATA_LEAF(CLASS)                                            \
  case Metadata::CLASS##Kind:                                                  \
    cast<CLASS>(OwnerMD)->handleChangedOperand(MDPtr, OrigOperand);            \
    break;
#include "llvm/IR/Metadata.def"
  default:
    llvm_unreachable("Invalid metadata subclass");
  }
}

#ifndef NDEBUG
void MetadataChangeOperand::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "MetadataChangeOperand\n";
}

void MetadataChangeOperand::dump() const { dump(dbgs()); }
#endif // NDEBUG

template <typename T>
DeleteObj<T>::DeleteObj(T *Ptr, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::DeleteObjID, CE), Ptr(Ptr) {}

template <typename T> void DeleteObj<T>::apply() { delete Ptr; }

#ifndef NDEBUG
template <typename T> void DeleteObj<T>::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteObj\n";
}

template <typename T> void DeleteObj<T>::dump() const { dump(dbgs()); }
#endif // NDEBUG

namespace llvm {
template class DeleteObj<MetadataAsValue>;
}

ClearMetadata::ClearMetadata(Value *Val, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::ClearMetadataID, CE) {
  // We would idially call Value::getAllMetadata() here, but it crashes if
  // Info.empty() so we are replicating its functionality.
  auto &ValueMetadata = Val->getContext().pImpl->ValueMetadata;
  auto It = ValueMetadata.find(Val);
  if (It == ValueMetadata.end())
    return;
  It->second.getAll(OrigMetadata);
}

void ClearMetadata::revert() {
  auto DisableGuard = Parent->disable();
  for (const auto &Pair : OrigMetadata) {
    unsigned KindID = Pair.first;
    MDNode *MD = Pair.second;
    V->addMetadata(KindID, *MD);
  }
}

#ifndef NDEBUG
void ClearMetadata::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "ClearMetadata:" << V << "\n";
}

void ClearMetadata::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetName::SetName(Value *Val, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::SetNameID, CE) {
  OrigName = Val->getName();
}

void SetName::revert() {
  auto DisableGuard = Parent->disable();
  V->setName(OrigName);
}

void SetName::apply() {}

#ifndef NDEBUG
void SetName::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetName: " << V << " OrigName='" << OrigName << "'\n";
}

void SetName::dump() const { dump(dbgs()); }
#endif // NDEBUG

TakeName::TakeName(Value *Val, Value *FromV, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::TakeNameID, CE), OrigName(Val->getName()),
      FromV(FromV) {}

void TakeName::revert() {
  auto DisableGuard = Parent->disable();
  std::string CurrName(V->getName());
  V->setName(OrigName);
  FromV->setName(CurrName);
}

#ifndef NDEBUG
void TakeName::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "TakeName\n";
}

void TakeName::dump() const { dump(dbgs()); }
#endif // NDEBUG

DestroyName::DestroyName(Value *Val, CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::DestroyNameID, CE) {}

void DestroyName::apply() { V->destroyValueName(); }

#ifndef NDEBUG
void DestroyName::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DestroyName\n";
}

void DestroyName::dump() const { dump(dbgs()); }
#endif // NDEBUG

InsertInstr::InsertInstr(Instruction *I, CheckpointEngine *CE)
    : ChangeBase(I, ChangeID::InsertInstrID, CE) {}

void InsertInstr::revert() { cast<Instruction>(V)->removeFromParent(); }

#ifndef NDEBUG
void InsertInstr::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "InsertInstr:" << getDump(V) << "\n";
}

void InsertInstr::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveInstr::RemoveInstr(Instruction *I, CheckpointEngine *CE)
    : ChangeBase(I, ChangeID::RemoveInstrID, CE) {
  PrevInstrOrBB = CheckpointEngine::getPrevInstrOrParent(I);
#ifndef NDEBUG
  // Add `I`, its defs and its users.
  addDump(I);
  for (Value *Op : I->operands())
    addDump(Op);
  for (User *U : I->users())
    addDump(U);
#endif
}

void ChkpntInstrUtils::insertAfter(Instruction *I, Value *PrevInstrOrBB) {
  if (isa<Instruction>(PrevInstrOrBB))
    I->insertAfter(cast<Instruction>(PrevInstrOrBB));
  else {
    BasicBlock *BB = cast<BasicBlock>(PrevInstrOrBB);
    BB->getInstList().insert(BB->begin(), I);
  }
}

void RemoveInstr::revert() { insertAfter(cast<Instruction>(V), PrevInstrOrBB); }

#ifndef NDEBUG
void RemoveInstr::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveInstr:" << getDump(V);
  if (isa<Instruction>(PrevInstrOrBB))
    OS << "  PrevI: " << getDump(PrevInstrOrBB);
  else
    OS << " AtTopOfBB: " << getDump(cast<BasicBlock>(PrevInstrOrBB));
  OS << "\n";
}

void RemoveInstr::dump() const { dump(dbgs()); }
#endif // NDEBUG

template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
AddToConstantUniqueMap<ConstantClass, ConstantUniqueMapTy, LookupKeyHashedTy>::
    AddToConstantUniqueMap(ConstantClass *CP, const LookupKeyHashedTy &Key,
                           ConstantUniqueMapTy *Map, CheckpointEngine *CE)
    : ChangeBase(CP, ChangeID::AddToConstantUniqueMapID, CE), Map(Map),
      Key(Key) {}

template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
void AddToConstantUniqueMap<ConstantClass, ConstantUniqueMapTy,
                            LookupKeyHashedTy>::revert() {
  Map->remove(cast<ConstantClass>(V));
}

#ifndef NDEBUG
template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
void AddToConstantUniqueMap<ConstantClass, ConstantUniqueMapTy,
                            LookupKeyHashedTy>::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "AddToConstantUniqueMap\n";
}

template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
void AddToConstantUniqueMap<ConstantClass, ConstantUniqueMapTy,
                            LookupKeyHashedTy>::dump() const {
  dump(dbgs());
}
#endif // NDEBUG

namespace llvm {
template class AddToConstantUniqueMap<
    ConstantArray, ConstantUniqueMap<ConstantArray>,
    ConstantUniqueMap<ConstantArray>::LookupKeyHashed>;
template class AddToConstantUniqueMap<
    ConstantStruct, ConstantUniqueMap<ConstantStruct>,
    ConstantUniqueMap<ConstantStruct>::LookupKeyHashed>;
template class AddToConstantUniqueMap<
    ConstantVector, ConstantUniqueMap<ConstantVector>,
    ConstantUniqueMap<ConstantVector>::LookupKeyHashed>;
template class AddToConstantUniqueMap<
    ConstantExpr, ConstantUniqueMap<ConstantExpr>,
    ConstantUniqueMap<ConstantExpr>::LookupKeyHashed>;
template class AddToConstantUniqueMap<
    InlineAsm, ConstantUniqueMap<InlineAsm>,
    ConstantUniqueMap<InlineAsm>::LookupKeyHashed>;
} // namespace llvm

template <typename ConstantClass, typename ConstantUniqueMapTy>
RemoveFromConstantUniqueMap<ConstantClass, ConstantUniqueMapTy>::
    RemoveFromConstantUniqueMap(ConstantClass *CP, ConstantUniqueMapTy *Map,
                                CheckpointEngine *CE)
    : ChangeBase(CP, ChangeID::RemoveFromConstantUniqueMapID, CE), Map(Map) {}

template <typename ConstantClass, typename ConstantUniqueMapTy>
void RemoveFromConstantUniqueMap<ConstantClass, ConstantUniqueMapTy>::revert() {
  Map->Map.insert(cast<ConstantClass>(V));
}

#ifndef NDEBUG
template <typename ConstantClass, typename ConstantUniqueMapTy>
void RemoveFromConstantUniqueMap<ConstantClass, ConstantUniqueMapTy>::dump(
    raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveFromConstantUniqueMap\n";
}

template <typename ConstantClass, typename ConstantUniqueMapTy>
void RemoveFromConstantUniqueMap<ConstantClass, ConstantUniqueMapTy>::dump()
    const {
  dump(dbgs());
}
#endif // NDEBUG
namespace llvm {
template class RemoveFromConstantUniqueMap<ConstantArray,
                                           ConstantUniqueMap<ConstantArray>>;
template class RemoveFromConstantUniqueMap<ConstantStruct,
                                           ConstantUniqueMap<ConstantStruct>>;
template class RemoveFromConstantUniqueMap<ConstantVector,
                                           ConstantUniqueMap<ConstantVector>>;
template class RemoveFromConstantUniqueMap<ConstantExpr,
                                           ConstantUniqueMap<ConstantExpr>>;
template class RemoveFromConstantUniqueMap<InlineAsm,
                                           ConstantUniqueMap<InlineAsm>>;
} // namespace llvm

template <typename MapClass>
AddToConstantMap<MapClass>::AddToConstantMap(typename MapClass::key_type Key,
                                             typename MapClass::mapped_type Val,
                                             MapClass *Map,
                                             CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::AddToConstantMapID, CE), Map(Map),
      Key(Key) {}

template <typename MapClass> void AddToConstantMap<MapClass>::revert() {
  Map->erase(Key);
}

#ifndef NDEBUG
template <typename MapClass>
void AddToConstantMap<MapClass>::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "AddToConstantMap\n";
}

template <typename MapClass> void AddToConstantMap<MapClass>::dump() const {
  dump(dbgs());
}
#endif // NDEBUG

namespace llvm {
template class AddToConstantMap<DenseMap<const GlobalValue *, NoCFIValue *>>;
template class AddToConstantMap<
    DenseMap<const GlobalValue *, DSOLocalEquivalent *>>;
template class AddToConstantMap<
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>>;
} // namespace llvm

template <typename MapClass>
RemoveFromConstantMap<MapClass>::RemoveFromConstantMap(
    typename MapClass::key_type Key, MapClass *Map, CheckpointEngine *CE)
    : ChangeBase(Map->find(Key)->second, ChangeID::RemoveFromConstantMapID, CE),
      Map(Map), Key(Key) {}

template <typename MapClass> void RemoveFromConstantMap<MapClass>::revert() {
  using ValT = typename MapClass::mapped_type;
  ValT OrigValue = cast<typename std::remove_pointer<ValT>::type>(V);
  Map->insert({Key, OrigValue});
}

#ifndef NDEBUG
template <typename MapClass>
void RemoveFromConstantMap<MapClass>::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveFromConstantMap\n";
}

template <typename MapClass>
void RemoveFromConstantMap<MapClass>::dump() const {
  dump(dbgs());
}
#endif // NDEBUG
namespace llvm {
template class RemoveFromConstantMap<
    DenseMap<const GlobalValue *, NoCFIValue *>>;
template class RemoveFromConstantMap<
    DenseMap<const GlobalValue *, DSOLocalEquivalent *>>;
template class RemoveFromConstantMap<
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>>;
} // namespace llvm

SetDebugLoc::SetDebugLoc(Instruction *I, CheckpointEngine *CE)
    : ChangeBase(I, ChangeID::DebugLocID, CE), OriginalLoc(I->getDebugLoc()) {}

void SetDebugLoc::revert() { cast<Instruction>(V)->setDebugLoc(OriginalLoc); }

#ifndef NDEBUG
void SetDebugLoc::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetDebugLoc:" << getDump(V) << " OrigLoc=" << OriginalLoc << "\n";
}
void SetDebugLoc::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetFnAttributes::SetFnAttributes(Function *F, CheckpointEngine *CE)
    : ChangeBase(F, ChangeID::SetFnAttributesID, CE),
      OrigAttrs(F->getAttributes()) {}

void SetFnAttributes::revert() { cast<Function>(V)->setAttributes(OrigAttrs); }

#ifndef NDEBUG
void SetFnAttributes::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetFnAttributes: " << getDump(V);
  OS << " Attr: ";
  OrigAttrs.print(OS);
  OS << "\n";
}
void SetFnAttributes::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetCallBaseAttributes::SetCallBaseAttributes(CallBase *C, CheckpointEngine *CE)
    : ChangeBase(C, ChangeID::SetCallBaseAttributesID, CE),
      OrigAttrs(C->getAttributes()) {}

void SetCallBaseAttributes::revert() {
  cast<CallBase>(V)->setAttributes(OrigAttrs);
}

#ifndef NDEBUG
void SetCallBaseAttributes::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetCallBaseAttributes: " << getDump(V);
  OS << " Attr: ";
  OrigAttrs.print(OS);
  OS << "\n";
}
void SetCallBaseAttributes::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetGlobalVariableAttributes::SetGlobalVariableAttributes(GlobalVariable *GV,
                                                         CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::SetGlobalVariableAttributesID, CE),
      OrigAttrSet(GV->getAttributes()) {}

void SetGlobalVariableAttributes::revert() {
  cast<GlobalVariable>(V)->setAttributes(OrigAttrSet);
}

#ifndef NDEBUG
void SetGlobalVariableAttributes::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetGlobalVariableAttributes: " << getDump(V) << "\n";
}
void SetGlobalVariableAttributes::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetGlobalVariableInitializer::SetGlobalVariableInitializer(GlobalVariable *GV,
                                                           CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::SetGlobalVariableInitializerID, CE),
      OrigInitVal(GV->getInitializer()) {}

void SetGlobalVariableInitializer::revert() {
  cast<GlobalVariable>(V)->setInitializer(OrigInitVal);
}

#ifndef NDEBUG
void SetGlobalVariableInitializer::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetGlobalVariableInitializer\n";
}
void SetGlobalVariableInitializer::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetGlobalVariableBits::SetGlobalVariableBits(GlobalVariable *GV,
                                             CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::SetGlobalVariableBitsID, CE),
      Bits({GV->isConstant(), GV->isExternallyInitialized()}) {}

void SetGlobalVariableBits::revert() {
  cast<GlobalVariable>(V)->setConstant(Bits.isConstantGlobal);
  cast<GlobalVariable>(V)->setExternallyInitialized(
      Bits.isExternallyInitializedConstant);
}

#ifndef NDEBUG
void SetGlobalVariableBits::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetGlobalVariableBits\n";
}
void SetGlobalVariableBits::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteGlobalVariable::DeleteGlobalVariable(GlobalVariable *GV,
                                           CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::DeleteGlobalVariableID, CE) {}

void DeleteGlobalVariable::apply() { delete cast<GlobalVariable>(V); }

#ifndef NDEBUG
void DeleteGlobalVariable::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteGlobalVariable: " << V << "\n";
}
void DeleteGlobalVariable::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveGlobalVariable::RemoveGlobalVariable(GlobalVariable *GV,
                                           CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::RemoveGlobalVariableID, CE) {
  auto It = GV->getIterator();
  if (GV->getIterator() != GV->getParent()->globals().begin())
    PrevGVOrModule = &*--It;
  else
    PrevGVOrModule = GV->getParent();
}

void RemoveGlobalVariable::revert() {
  if (GlobalVariable **PrevGV =
          std::get_if<GlobalVariable *>(&PrevGVOrModule)) {
    auto &List = (*PrevGV)->getParent()->getGlobalList();
    List.insertAfter((*PrevGV)->getIterator(), cast<GlobalVariable>(V));
  } else {
    Module *M = std::get<Module *>(PrevGVOrModule);
    auto &List = M->getGlobalList();
    List.insert(List.begin(), cast<GlobalVariable>(V));
  }
}

#ifndef NDEBUG
void RemoveGlobalVariable::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveGlobalVariable: " << V << "\n";
}
void RemoveGlobalVariable::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveGlobalAlias::RemoveGlobalAlias(GlobalAlias *GA, CheckpointEngine *CE)
    : ChangeBase(GA, ChangeID::RemoveGlobalAliasID, CE) {
  auto It = GA->getIterator();
  if (GA->getIterator() != GA->getParent()->getAliasList().begin())
    PrevGVOrModule = &*--It;
  else
    PrevGVOrModule = GA->getParent();
}

void RemoveGlobalAlias::revert() {
  if (GlobalAlias **PrevGV = std::get_if<GlobalAlias *>(&PrevGVOrModule)) {
    auto &List = (*PrevGV)->getParent()->getAliasList();
    List.insertAfter((*PrevGV)->getIterator(), cast<GlobalAlias>(V));
  } else {
    Module *M = std::get<Module *>(PrevGVOrModule);
    auto &List = M->getAliasList();
    List.insert(List.begin(), cast<GlobalAlias>(V));
  }
}

#ifndef NDEBUG
void RemoveGlobalAlias::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveGlobalAlias: " << V << "\n";
}
void RemoveGlobalAlias::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteGlobalAlias::DeleteGlobalAlias(GlobalAlias *GV, CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::DeleteGlobalAliasID, CE) {}

void DeleteGlobalAlias::apply() {
  auto DisableGuard = Parent->disable();
  delete cast<GlobalAlias>(V);
}

#ifndef NDEBUG
void DeleteGlobalAlias::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteGlobalAlias: " << V << "\n";
}
void DeleteGlobalAlias::dump() const { dump(dbgs()); }
#endif // NDEBUG

InsertGlobalAlias::InsertGlobalAlias(GlobalAlias *GV, CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::InsertGlobalAliasID, CE) {}

void InsertGlobalAlias::revert() { cast<GlobalAlias>(V)->removeFromParent(); }

#ifndef NDEBUG
void InsertGlobalAlias::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "InsertGlobalAlias: " << V << "\n";
}
void InsertGlobalAlias::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveGlobalIFunc::RemoveGlobalIFunc(GlobalIFunc *GIF, CheckpointEngine *CE)
    : ChangeBase(GIF, ChangeID::RemoveGlobalIFuncID, CE) {

  auto It = GIF->getIterator();
  if (GIF->getIterator() != GIF->getParent()->ifuncs().begin())
    PrevGVOrModule = &*--It;
  else
    PrevGVOrModule = GIF->getParent();
}

void RemoveGlobalIFunc::revert() {
  if (GlobalIFunc **PrevGV = std::get_if<GlobalIFunc *>(&PrevGVOrModule)) {
    auto &List = (*PrevGV)->getParent()->getIFuncList();
    List.insertAfter((*PrevGV)->getIterator(), cast<GlobalIFunc>(V));
  } else {
    Module *M = std::get<Module *>(PrevGVOrModule);
    auto &List = M->getIFuncList();
    List.insert(List.begin(), cast<GlobalIFunc>(V));
  }
}

#ifndef NDEBUG
void RemoveGlobalIFunc::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveGlobalIFunc: " << V << "\n";
}
void RemoveGlobalIFunc::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteGlobalIFunc::DeleteGlobalIFunc(GlobalIFunc *GIF, CheckpointEngine *CE)
    : ChangeBase(GIF, ChangeID::DeleteGlobalIFuncID, CE) {}

void DeleteGlobalIFunc::apply() {
  auto DisableGuard = Parent->disable();
  delete cast<GlobalIFunc>(V);
}

#ifndef NDEBUG
void DeleteGlobalIFunc::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteGlobalIFunc: " << V << "\n";
}
void DeleteGlobalIFunc::dump() const { dump(dbgs()); }
#endif // NDEBUG

InsertGlobalIFunc::InsertGlobalIFunc(GlobalIFunc *GV, CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::InsertGlobalIFuncID, CE) {}

void InsertGlobalIFunc::revert() { cast<GlobalIFunc>(V)->removeFromParent(); }

#ifndef NDEBUG
void InsertGlobalIFunc::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "InsertGlobalIFunc: " << V << "\n";
}
void InsertGlobalIFunc::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveNamedMDNode::RemoveNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::RemoveNamedMDNodeID, CE), RemovedNode(MDN) {
  auto It = MDN->getIterator();
  if (MDN->getIterator() != MDN->getParent()->named_metadata().begin())
    PrevGVOrModule = &*--It;
  else
    PrevGVOrModule = MDN->getParent();
}

void RemoveNamedMDNode::revert() {
  if (NamedMDNode **PrevGV = std::get_if<NamedMDNode *>(&PrevGVOrModule)) {
    auto &List = (*PrevGV)->getParent()->getNamedMDList();
    List.insertAfter((*PrevGV)->getIterator(), RemovedNode);
  } else {
    Module *M = std::get<Module *>(PrevGVOrModule);
    auto &List = M->getNamedMDList();
    List.insert(List.begin(), RemovedNode);
  }
}

#ifndef NDEBUG
void RemoveNamedMDNode::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveNamedMDNode: " << RemovedNode << "\n";
}
void RemoveNamedMDNode::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteNamedMDNode::DeleteNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::DeleteNamedMDNodeID, CE), DeletedNode(MDN) {
}

void DeleteNamedMDNode::apply() {
  auto DisableGuard = Parent->disable();
  delete DeletedNode;
}

#ifndef NDEBUG
void DeleteNamedMDNode::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteNamedMDNode: " << DeletedNode << "\n";
}
void DeleteNamedMDNode::dump() const { dump(dbgs()); }
#endif // NDEBUG

InsertNamedMDNode::InsertNamedMDNode(NamedMDNode *MDN, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::InsertNamedMDNodeID, CE),
      InsertedNode(MDN) {}

void InsertNamedMDNode::revert() {
  InsertedNode->getParent()->getNamedMDList().remove(InsertedNode);
}

#ifndef NDEBUG
void InsertNamedMDNode::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "InsertNamedMDNode: " << InsertedNode << "\n";
}
void InsertNamedMDNode::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetComdat::SetComdat(GlobalObject *GO, CheckpointEngine *CE)
    : ChangeBase(GO, ChangeID::SetComdatID, CE) {
  OrigComdat = GO->getComdat();
}

void SetComdat::revert() { cast<GlobalObject>(V)->setComdat(OrigComdat); }

#ifndef NDEBUG
void SetComdat::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetComdat: " << V << "\n";
}
void SetComdat::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetOperand::SetOperand(User *U, uint32_t OpIdx, CheckpointEngine *CE)
    : ChangeBase(U, ChangeID::SetOperandID, CE), OpIdx(OpIdx),
      Op(U->getOperand(OpIdx)) {}

void SetOperand::revert() { cast<User>(V)->setOperand(OpIdx, Op); }

#ifndef NDEBUG
void SetOperand::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetOperand:" << getDump(V);
  OS << "    OpIdx=" << OpIdx << " Op=";
  if (Op != nullptr)
    OS << getDump(Op);
  else
    OS << "NULL";
  OS << "\n";
}

void SetOperand::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetShuffleMask::SetShuffleMask(ShuffleVectorInst *Shuffle, CheckpointEngine *CE)
    : ChangeBase(Shuffle, ChangeID::SetShuffleMaskID, CE),
      OrigMask(Shuffle->getShuffleMask()) {}

void SetShuffleMask::revert() {
  auto DisableGuard = Parent->disable();
  cast<ShuffleVectorInst>(V)->setShuffleMask(OrigMask);
}

#ifndef NDEBUG
void SetShuffleMask::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetShuffleMask: " << getDump(V) << " OrigMask: ";
  for (int Elem : OrigMask)
    OS << Elem << ", ";
  OS << "\n";
}

void SetShuffleMask::dump() const { dump(dbgs()); }
#endif // NDEBUG

SwapUse::SwapUse(Use *U1, Use *U2, CheckpointEngine *CE)
    : ChangeBase(nullptr, ChangeID::SwapUseID, CE), U1(U1), U2(U2) {}

void SwapUse::revert() { U1->swap(*U2); }

#ifndef NDEBUG
void SwapUse::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SwapUse\n";
}

void SwapUse::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetIncomingBlocks::SetIncomingBlocks(PHINode *Phi, uint32_t Idx,
                                     uint32_t NumBlocks, CheckpointEngine *CE)
    : ChangeBase(Phi, ChangeID::SetIncomingBlocksID, CE), Idx(Idx),
      OrigBBs(Phi->block_begin() + Idx, Phi->block_begin() + Idx + NumBlocks) {}

void SetIncomingBlocks::revert() {
  for (auto OrigBBIdx : seq<uint32_t>(0, OrigBBs.size())) {
    BasicBlock *OrigBB = OrigBBs[OrigBBIdx];
    uint32_t InputBBIdx = OrigBBIdx + Idx;
    // When the PHI gets assigned an input BB for the first time it can be null.
    if (OrigBB != nullptr)
      cast<PHINode>(V)->setIncomingBlock(InputBBIdx, OrigBB);
  }
}

#ifndef NDEBUG
void SetIncomingBlocks::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetIncomingBlocks: " << Idx << " (" << OrigBBs.size() << " bbs)\n";
}

void SetIncomingBlocks::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetNumUserOperands::SetNumUserOperands(User *U, uint32_t NumOps,
                                       CheckpointEngine *CE)
    : ChangeBase(U, ChangeID::SetNumUserOperandsID, CE), NumOps(NumOps) {}

void SetNumUserOperands::revert() { cast<User>(V)->NumUserOperands = NumOps; }

#ifndef NDEBUG
void SetNumUserOperands::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetNumUserOperands: " << NumOps << "\n";
}
void SetNumUserOperands::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetSubclassData::SetSubclassData(Value *Val, uint16_t OrigData,
                                 CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::SetSubclassDataID, CE), OrigData(OrigData) {}

void SetSubclassData::revert() { V->SubclassData = OrigData; }

#ifndef NDEBUG
void SetSubclassData::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetSubclassData: " << OrigData << "\n";
}
void SetSubclassData::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetSubclassOptionalData::SetSubclassOptionalData(Value *Val, uint16_t OrigData,
                                                 CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::SetSubclassOptionalDataID, CE),
      OrigData(OrigData) {}

void SetSubclassOptionalData::revert() { V->SubclassOptionalData = OrigData; }

#ifndef NDEBUG
void SetSubclassOptionalData::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetSubclassOptionalData: " << OrigData << "\n";
}
void SetSubclassOptionalData::dump() const { dump(dbgs()); }
#endif // NDEBUG

SetGlobalValueSubClassData::SetGlobalValueSubClassData(Value *Val,
                                                       uint16_t OrigData,
                                                       CheckpointEngine *CE)
    : ChangeBase(Val, ChangeID::SetGlobalValueSubClassDataID, CE),
      OrigData(OrigData) {}

void SetGlobalValueSubClassData::revert() {
  auto DisableGuard = Parent->disable();
  cast<GlobalValue>(V)->setGlobalValueSubClassData(OrigData);
}

#ifndef NDEBUG
void SetGlobalValueSubClassData::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "SetGlobalValueSubClassData: " << OrigData << "\n";
}
void SetGlobalValueSubClassData::dump() const { dump(dbgs()); }
#endif // NDEBUG

GlobalValueBitfield::GlobalValueBitfield(GlobalValue *GV, CheckpointEngine *CE)
    : ChangeBase(GV, ChangeID::GlobalValueBitfieldID, CE) {
  OrigBitfield = GV->getAsInt();
}

void GlobalValueBitfield::revert() {
  cast<GlobalValue>(V)->setFromInt(OrigBitfield);
}

#ifndef NDEBUG
void GlobalValueBitfield::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "GlobalValueBitfield " << V << "\n";
}
void GlobalValueBitfield::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveBB::RemoveBB(BasicBlock *BB, CheckpointEngine *CE)
    : ChangeBase(BB, ChangeID::RemoveBBID, CE) {
  F = BB->getParent();
  NextBB = BB->getNextNode();
#ifndef NDEBUG
  for (User *U : BB->users())
    addDump(U);
#endif
}

void RemoveBB::revert() { cast<BasicBlock>(V)->insertInto(F, NextBB); }

#ifndef NDEBUG
void RemoveBB::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveBB:" << cast<BasicBlock>(V)->getName();
  OS << "   F:" << F->getName() << " NextBB=";
  if (NextBB != nullptr)
    OS << NextBB->getName();
  else
    OS << "F.end()";
  OS << "\n";
}
void RemoveBB::dump() const { dump(dbgs()); }
#endif // NDEBUG

MoveBB::MoveBB(BasicBlock *BB, CheckpointEngine *CE)
    : ChangeBase(BB, ChangeID::MoveBBID, CE) {
  if (BasicBlock *PrevBB = BB->getPrevNode())
    PrevBBOrFn = PrevBB;
  else
    PrevBBOrFn = BB->getParent();
}

void MoveBB::revert() {
  if (auto *PrevBB = dyn_cast<BasicBlock>(PrevBBOrFn))
    cast<BasicBlock>(V)->moveAfter(PrevBB);
  else {
    BasicBlock *FirstBB = &*cast<Function>(PrevBBOrFn)->begin();
    cast<BasicBlock>(V)->moveBefore(FirstBB);
  }
}

#ifndef NDEBUG
void MoveBB::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "MoveBB:" << cast<BasicBlock>(V)->getName();
  if (auto *PrevBB = dyn_cast<BasicBlock>(PrevBBOrFn))
    OS << "   PrevBB=" << getDump(PrevBB);
  else {
    auto *Fn = cast<Function>(PrevBBOrFn);
    BasicBlock *FirstBB = &*Fn->begin();
    OS << "   NextBB=" << FirstBB->getName();
  }
  OS << "\n";
}
void MoveBB::dump() const { dump(dbgs()); }
#endif // NDEBUG

SpliceBB::SpliceBB(Value *OrigInstrOrBB, Instruction *FirstI,
                   Instruction *LastI, CheckpointEngine *CE)
    : ChangeBase(OrigInstrOrBB, ChangeID::SpliceBBID, CE), FirstI(FirstI),
      LastI(LastI) {}

void SpliceBB::revert() {
  BasicBlock *OrigBB;
  BasicBlock::iterator OrigIt;
  if (isa<BasicBlock>(V)) {
    OrigBB = cast<BasicBlock>(V);
    OrigIt = OrigBB->begin();
  } else {
    OrigBB = cast<Instruction>(V)->getParent();
    OrigIt = std::next(cast<Instruction>(V)->getIterator());
  }
  OrigBB->getInstList().splice(
      OrigIt, FirstI->getParent()->getInstList(), FirstI->getIterator(),
      LastI != nullptr ? std::next(LastI->getIterator())
                       : FirstI->getIterator());
}
#ifndef NDEBUG
void SpliceBB::dump(raw_ostream &OS) const {
  OS << "SpliceBB:";
  if (isa<BasicBlock>(V))
    OS << " OrigPos=Top of: " << cast<BasicBlock>(V)->getName() << "\n";
  else
    OS << " OrigPos=" << getDump(V) << "\n";
  OS << "  FirstI:" << getDump(FirstI) << "\n";
  OS << "  LastI:" << getDump(LastI) << "\n";
}
void SpliceBB::dump() const { dump(dbgs()); }
#endif // NDEBUG

InsertBB::InsertBB(BasicBlock *NewBB, CheckpointEngine *CE)
    : ChangeBase(NewBB, ChangeID::InsertBBID, CE) {}

void InsertBB::revert() { cast<BasicBlock>(V)->removeFromParent(); }

#ifndef NDEBUG
void InsertBB::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "InsertBB: " << cast<BasicBlock>(V)->getName() << "\n";
}
void InsertBB::dump() const { dump(dbgs()); }
#endif // NDEBUG

SpliceFn::SpliceFn(Value *OrigBBOrFn, BasicBlock *FirstBB, BasicBlock *LastBB,
                   CheckpointEngine *CE)
    : ChangeBase(OrigBBOrFn, ChangeID::SpliceFnID, CE), FirstBB(FirstBB),
      LastBB(LastBB) {}

void SpliceFn::revert() {
  Function *OrigFn;
  Function::iterator OrigIt;
  if (isa<Function>(V)) {
    OrigFn = cast<Function>(V);
    OrigIt = OrigFn->begin();
  } else {
    OrigFn = cast<BasicBlock>(V)->getParent();
    OrigIt = std::next(cast<BasicBlock>(V)->getIterator());
  }
  OrigFn->getBasicBlockList().splice(
      OrigIt, FirstBB->getParent()->getBasicBlockList(), FirstBB->getIterator(),
      LastBB != nullptr ? std::next(LastBB->getIterator())
                        : FirstBB->getIterator());
}
#ifndef NDEBUG
void SpliceFn::dump(raw_ostream &OS) const {
  OS << "SpliceFn:";
  if (isa<Function>(V))
    OS << " OrigPos=Top of: " << getDump(cast<Function>(V)) << "\n";
  else
    OS << " OrigPos=" << getDump(V) << "\n";
  OS << "  FirstBB:" << getDump(FirstBB) << "\n";
  OS << "  LastBB:" << getDump(LastBB) << "\n";
}
void SpliceFn::dump() const { dump(dbgs()); }
#endif // NDEBUG

RemoveFn::RemoveFn(Function *Fn, CheckpointEngine *CE)
    : ChangeBase(Fn, ChangeID::RemoveFnID, CE) {
  M = Fn->getParent();
  auto NextIt = std::next(Fn->getIterator());
  NextFn = NextIt != M->getFunctionList().end() ? &*NextIt : nullptr;
}

void RemoveFn::revert() {
  Function *F = cast<Function>(V);
  auto Where =
      NextFn != nullptr ? NextFn->getIterator() : M->getFunctionList().end();
  M->getFunctionList().insert(Where, F);
}

#ifndef NDEBUG
void RemoveFn::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "RemoveFn: " << getDump(cast<Function>(V)) << "\n";
}
void RemoveFn::dump() const { dump(dbgs()); }
#endif // NDEBUG

CreateValue::CreateValue(Value *NewV, CheckpointEngine *CE)
    : ChangeBase(NewV, ChangeID::CreateValueID, CE) {}

// Deletes \p V if possible.
static void tryDeleteValue(Value *V) {
  auto *GV = dyn_cast<GlobalValue>(V);
  if (GV != nullptr && GV->getParent() != nullptr) {
    GV->eraseFromParent();
  } else if (auto *C = dyn_cast<Constant>(V)) {
    // TODO: Ideally we would call `C->destroyConstant()` here, but:
    // (i) ConstantTokenNone, ConstantFP and ConstantInt don't support it.
    // (ii) Others that do (like Poison) will crash when destroyConstant() is
    // called, possibly a bug?
  } else
    V->deleteValue();
}

void CreateValue::revert() { tryDeleteValue(V); }

#ifndef NDEBUG
void CreateValue::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "CreateValue: " << V << " " << getDump(V) << "\n";
}
void CreateValue::dump() const { dump(dbgs()); }
#endif // NDEBUG

DeleteValue::DeleteValue(Value *DelV, CheckpointEngine *CE)
    : ChangeBase(DelV, ChangeID::DeleteValueID, CE) {}

void DeleteValue::apply() { tryDeleteValue(V); }

#ifndef NDEBUG
void DeleteValue::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DeleteValue: " << getDump(V) << "\n";
}
void DeleteValue::dump() const { dump(dbgs()); }
#endif // NDEBUG

DestroyConstant::DestroyConstant(Constant *C, CheckpointEngine *CE)
    : ChangeBase(C, ChangeID::DestroyConstantID, CE) {}

void DestroyConstant::apply() { deleteConstant(cast<Constant>(V)); }

#ifndef NDEBUG
void DestroyConstant::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "DestroyConstant: " << getDump(V) << "\n";
}
void DestroyConstant::dump() const { dump(dbgs()); }
#endif // NDEBUG

ClearInstList::ClearInstList(BasicBlock *BB, CheckpointEngine *CE)
    : ChangeBase(BB, ChangeID::ClearInstListID, CE) {
  auto DisableChkpntGuard = Parent->disable();
  TmpBB = std::unique_ptr<BasicBlock>(
      BasicBlock::Create(BB->getContext(), "ChkpntTmpBB"));
  TmpBB->splice(TmpBB->begin(), BB);
}

void ClearInstList::revert() {
  BasicBlock *BB = cast<BasicBlock>(V);
  assert(BB->InstList.empty() && "Expected empty BB.");
  BB->InstList.splice(BB->InstList.begin(), TmpBB->InstList);
  TmpBB.release()->deleteValue();
}

void ClearInstList::apply() { TmpBB.release()->deleteValue(); }

#ifndef NDEBUG
void ClearInstList::dump(raw_ostream &OS) const {
  dumpCommon(OS);
  OS << "ClearInstList: " << getDump(V) << "\n";
}
void ClearInstList::dump() const { dump(dbgs()); }
#endif // NDEBUG
