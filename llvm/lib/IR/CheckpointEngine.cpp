//===- CheckpointEngine.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/CheckpointEngine.h"
#include "CheckpointInternal.h"
#include "LLVMContextImpl.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include <sstream>

using namespace llvm;
using namespace std;

#ifndef NDEBUG
std::string IRChecker::dumpIR() const {
  std::string TmpStr;
  raw_string_ostream SS(TmpStr);
  M->print(SS, /*AssemblyAnnotationWriter=*/nullptr);
  return SkipPreds
             ? std::regex_replace(TmpStr, PredsRegex, "; preds = <removed>\n")
             : TmpStr;
}

IRChecker::IRChecker(bool SkipPreds) : SkipPreds(SkipPreds) {}

IRChecker::IRChecker(Module &M, bool SkipPreds) : IRChecker(SkipPreds) {
  this->M = &M;
  save();
}

void IRChecker::save() {
  if (SkipPreds)
    PredsRegex = std::regex(PredsRegexStr);
  OriginalIR = dumpIR();
}

void IRChecker::save(Module &M) {
  this->M = &M;
  save();
}

void IRChecker::showDiff(const std::string &OrigIR, const std::string &CurrIR) {
  // Show the first line that differes.
  std::stringstream OrigSS(OrigIR);
  std::stringstream CurrSS(CurrIR);
  std::string OrigLine;
  std::string CurrLine;
  SmallVector<std::string> Context;
  static constexpr const uint32_t MaxContext = 3;
  while (OrigSS.good() && CurrSS.good()) {
    std::getline(OrigSS, OrigLine);
    std::getline(CurrSS, CurrLine);
    if (CurrLine != OrigLine) {
      // Print context.
      for (const std::string &ContextLine : Context)
        dbgs() << "  " << ContextLine << "\n";
      // Print the line difference.
      dbgs() << "- " << OrigLine << "\n";
      dbgs() << "+ " << CurrLine << "\n";
    } else {
      // Lazy way to maintain context. Performance of this code does not matter.
      Context.push_back(OrigLine);
      if (Context.size() > MaxContext)
        Context.erase(Context.begin());
    }
  }
  // If one file is larger than the other print line in the larger one.
  if (!OrigSS.good() && CurrSS.good()) {
    std::getline(CurrSS, CurrLine);
    dbgs() << "+ " << CurrLine << "\n";
  }
  if (OrigSS.good() && !CurrSS.good()) {
    std::getline(OrigSS, OrigLine);
    dbgs() << "+ " << OrigLine << "\n";
  }
}

void IRChecker::expectNoDiff() const {
  const std::string &OrigIR = origIR();
  std::string CurrIR = currIR();
  bool Same = OrigIR == CurrIR;
  if (!Same) {
    showDiff(OrigIR, CurrIR);
    llvm_unreachable(
        "Original and current IR differ! Possibly a Checkpointing bug.");
  }
}

void ValueDump::add(Value *V) {
  std::string &Str = Map[V];
  raw_string_ostream SS(Str);
  if (V == nullptr) {
    SS << "null";
    return;
  }
  if (auto *BB = dyn_cast<BasicBlock>(V)) {
    SS << BB->getName();
    return;
  }
  if (auto *F = dyn_cast<Function>(V)) {
    F->printAsOperand(SS);
    SS << "(";
    for (const auto &Arg : F->args()) {
      Arg.printAsOperand(SS);
      SS << ", ";
    }
    SS << ")";
    return;
  }
  SS << *V;
}
std::string ValueDump::get(Value *V) const {
  auto It = Map.find(V);
  if (It != Map.end())
    return It->second;
  std::string Str;
  raw_string_ostream SS(Str);
  if (auto *F = dyn_cast<Function>(V)) {
    F->printAsOperand(SS);
    SS << "(";
    for (const auto &Arg : F->args())
      SS << Arg << ", ";
    SS << ")";
  }
  else if (isa<BasicBlock>(V))
    SS << V->getName();
  else
    SS << V;  // Print the pointer value to be on the safe side
  return Str;
}
#endif // NDEBUG

CheckpointGuard::CheckpointGuard(bool NewState, CheckpointEngine *Chkpnt)
    : Chkpnt(Chkpnt), LastState(Chkpnt->Active) {
  Chkpnt->Active = NewState;
}

CheckpointGuard::~CheckpointGuard() { Chkpnt->Active = LastState; }

CheckpointEngine::CheckpointEngine() : Active(false) {}

CheckpointEngine::~CheckpointEngine() {
  assert(!Active && "Checkpoint should have taken care of this");
}

Value *CheckpointEngine::getPrevInstrOrParent(Instruction *I) {
  Instruction *PrevI = I->getPrevNode();
  return PrevI != nullptr ? static_cast<Value *>(PrevI)
                          : static_cast<Value *>(I->getParent());
}

Value *CheckpointEngine::getPrevBBOrParent(BasicBlock *BB) {
  BasicBlock *PrevBB = BB->getPrevNode();
  return PrevBB != nullptr ? static_cast<Value *>(PrevBB)
                           : static_cast<Value *>(BB->getParent());
}

void CheckpointEngine::clear() {
  Changes.clear();
#ifndef NDEBUG
  ChangeUids.clear();
#endif // NDEBUG
}

void CheckpointEngine::setMetadata(Value *V, unsigned KindID) {
  Changes.push_back(make_unique<SetMetadata>(V, KindID, this));
}

void CheckpointEngine::addMetadata(Value *V, unsigned KindID) {
  Changes.push_back(make_unique<AddMetadata>(V, KindID, this));
}

void CheckpointEngine::eraseMetadata(Value *V, unsigned KindID) {
  Changes.push_back(make_unique<EraseMetadata>(V, KindID, this));
}

void CheckpointEngine::changeMetadata(MetadataAsValue *MAV, Metadata *OrigMD) {
  Changes.push_back(make_unique<ChangeMetadata>(MAV, OrigMD, this));
}

void CheckpointEngine::deleteMetadata(Metadata *MD) {
  Changes.push_back(make_unique<DeleteMetadata>(MD, this));
}

void CheckpointEngine::handleRAUWMetadata(Value *From, Value *To) {
  Changes.push_back(make_unique<HandleRAUWMetadata>(From, To, this));
}

void CheckpointEngine::metadataUpdateUseMap(ReplaceableMetadataImpl *Def,
                                            Metadata **OrigMDPtr,
                                            uint64_t UseNum) {
  Changes.push_back(
      make_unique<MetadataUpdateUseMap>(Def, OrigMDPtr, UseNum, this));
}

void CheckpointEngine::metadataChangeOperand(Metadata *OwnerMD,
                                             Metadata **MDPtr) {
  Changes.push_back(make_unique<MetadataChangeOperand>(OwnerMD, MDPtr, this));
}

template <typename T> void CheckpointEngine::deleteObj(T *Ptr) {
  Changes.push_back(make_unique<DeleteObj<T>>(Ptr, this));
}

namespace llvm {
template void CheckpointEngine::deleteObj<MetadataAsValue>(MetadataAsValue *);
}

void CheckpointEngine::clearMetadata(Value *V) {
  Changes.push_back(make_unique<ClearMetadata>(V, this));
}

void CheckpointEngine::setName(Value *V) {
  Changes.push_back(make_unique<SetName>(V, this));
}

void CheckpointEngine::takeName(Value *V, Value *FromV) {
  Changes.push_back(make_unique<TakeName>(V, FromV, this));
}

void CheckpointEngine::destroyName(Value *V) {
  Changes.push_back(make_unique<DestroyName>(V, this));
}

void CheckpointEngine::insertInstr(Instruction *I) {
  Changes.push_back(make_unique<InsertInstr>(I, this));
}

void CheckpointEngine::removeInstr(Instruction *I) {
  Changes.push_back(make_unique<RemoveInstr>(I, this));
}

template <typename ConstantClass, typename ConstantUniqueMapTy,
          typename LookupKeyHashedTy>
void CheckpointEngine::addToConstantUniqueMap(ConstantClass *C,
                                              const LookupKeyHashedTy &Key,
                                              ConstantUniqueMapTy *Map) {
  Changes.push_back(
      make_unique<AddToConstantUniqueMap<ConstantClass, ConstantUniqueMapTy,
                                         LookupKeyHashedTy>>(C, Key, Map, this));
}

template <typename ConstantClass, typename ConstantUniqueMapTy>
void CheckpointEngine::removeFromConstantUniqueMap(ConstantClass *C,
                                                   ConstantUniqueMapTy *Map) {
  Changes.push_back(
      make_unique<
          RemoveFromConstantUniqueMap<ConstantClass, ConstantUniqueMapTy>>(
          C, Map, this));
}

namespace llvm {
template void CheckpointEngine::addToConstantUniqueMap<
    ConstantArray, ConstantUniqueMap<ConstantArray>,
    ConstantUniqueMap<ConstantArray>::LookupKeyHashed>(
    ConstantArray *, const ConstantUniqueMap<ConstantArray>::LookupKeyHashed &,
    ConstantUniqueMap<ConstantArray> *);

template void CheckpointEngine::addToConstantUniqueMap<
    ConstantStruct, ConstantUniqueMap<ConstantStruct>,
    ConstantUniqueMap<ConstantStruct>::LookupKeyHashed>(
    ConstantStruct *, const ConstantUniqueMap<ConstantStruct>::LookupKeyHashed &,
    ConstantUniqueMap<ConstantStruct> *);

template void CheckpointEngine::addToConstantUniqueMap<
    ConstantVector, ConstantUniqueMap<ConstantVector>,
    ConstantUniqueMap<ConstantVector>::LookupKeyHashed>(
    ConstantVector *, const ConstantUniqueMap<ConstantVector>::LookupKeyHashed &,
    ConstantUniqueMap<ConstantVector> *);

template void CheckpointEngine::addToConstantUniqueMap<
    ConstantExpr, ConstantUniqueMap<ConstantExpr>,
    ConstantUniqueMap<ConstantExpr>::LookupKeyHashed>(
    ConstantExpr *, const ConstantUniqueMap<ConstantExpr>::LookupKeyHashed &,
    ConstantUniqueMap<ConstantExpr> *);

template void CheckpointEngine::addToConstantUniqueMap<
    InlineAsm, ConstantUniqueMap<InlineAsm>,
    ConstantUniqueMap<InlineAsm>::LookupKeyHashed>(
    InlineAsm *, const ConstantUniqueMap<InlineAsm>::LookupKeyHashed &,
    ConstantUniqueMap<InlineAsm> *);

template void
CheckpointEngine::removeFromConstantUniqueMap<ConstantArray,
                                              ConstantUniqueMap<ConstantArray>>(
    ConstantArray *, ConstantUniqueMap<ConstantArray> *);

template void CheckpointEngine::removeFromConstantUniqueMap<
    ConstantStruct, ConstantUniqueMap<ConstantStruct>>(
    ConstantStruct *, ConstantUniqueMap<ConstantStruct> *);

template void CheckpointEngine::removeFromConstantUniqueMap<
    ConstantVector, ConstantUniqueMap<ConstantVector>>(
    ConstantVector *, ConstantUniqueMap<ConstantVector> *);

template void
CheckpointEngine::removeFromConstantUniqueMap<ConstantExpr,
                                              ConstantUniqueMap<ConstantExpr>>(
    ConstantExpr *, ConstantUniqueMap<ConstantExpr> *);

template void CheckpointEngine::removeFromConstantUniqueMap<
    InlineAsm, ConstantUniqueMap<InlineAsm>>(InlineAsm *,
                                             ConstantUniqueMap<InlineAsm> *);
}

template <typename MapClass>
void CheckpointEngine::addToConstantMap(typename MapClass::key_type Key,
                                        typename MapClass::mapped_type Val,
                                        MapClass *Map) {
  Changes.push_back(
      make_unique<AddToConstantMap<MapClass>>(Key, Val, Map, this));
}

template <typename MapClass>
void CheckpointEngine::removeFromConstantMap(typename MapClass::key_type Key,
                                             MapClass *Map) {
  Changes.push_back(
      make_unique<RemoveFromConstantMap<MapClass>>(Key, Map, this));
}

namespace llvm {
template void
CheckpointEngine::addToConstantMap<DenseMap<const GlobalValue *, NoCFIValue *>>(
    const GlobalValue *, NoCFIValue *,
    DenseMap<const GlobalValue *, NoCFIValue *> *);

template void CheckpointEngine::addToConstantMap<
    DenseMap<const GlobalValue *, DSOLocalEquivalent *>>(
    const GlobalValue *, DSOLocalEquivalent *,
    DenseMap<const GlobalValue *, DSOLocalEquivalent *> *);

template void CheckpointEngine::addToConstantMap<
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>>(
    std::pair<const Function *, const BasicBlock *>, BlockAddress *,
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>
        *);

template void CheckpointEngine::removeFromConstantMap<
    DenseMap<const GlobalValue *, NoCFIValue *>>(
    const GlobalValue *, DenseMap<const GlobalValue *, NoCFIValue *> *);

template void CheckpointEngine::removeFromConstantMap<
    DenseMap<const GlobalValue *, DSOLocalEquivalent *>>(
    const GlobalValue *, DenseMap<const GlobalValue *, DSOLocalEquivalent *> *);

template void CheckpointEngine::removeFromConstantMap<
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>>(
    std::pair<const Function *, const BasicBlock *>,
    DenseMap<std::pair<const Function *, const BasicBlock *>, BlockAddress *>
        *);
}

void CheckpointEngine::setOperand(User *U, uint32_t OpIdx) {
  Changes.push_back(make_unique<SetOperand>(U, OpIdx, this));
}

void CheckpointEngine::setShuffleMask(ShuffleVectorInst *Shuffle) {
  Changes.push_back(make_unique<SetShuffleMask>(Shuffle, this));
}

void CheckpointEngine::swapUse(Use *U1, Use *U2) {
  Changes.push_back(make_unique<SwapUse>(U1, U2, this));
}

void CheckpointEngine::setIncomingBlocks(PHINode *Phi, uint32_t Idx,
                                         uint32_t NumBlocks) {
  Changes.push_back(make_unique<SetIncomingBlocks>(Phi, Idx, NumBlocks, this));
}

void CheckpointEngine::setNumUserOperands(User *U, uint32_t NumUserOperands) {
  Changes.push_back(make_unique<SetNumUserOperands>(U, NumUserOperands, this));
}

void CheckpointEngine::setSubclassData(Value *V, uint16_t Data) {
  // SubclassData is used for finding a ConstantExpr in the Map. If we revert
  // it, then destroying the value crashes.
  if (isa<ConstantExpr>(V))
    return;
  Changes.push_back(make_unique<SetSubclassData>(V, Data, this));
}

void CheckpointEngine::setSubclassOptionalData(Value *V, uint16_t Data) {
  Changes.push_back(make_unique<SetSubclassOptionalData>(V, Data, this));
}

void CheckpointEngine::setGlobalValueSubClassData(GlobalValue *GV, uint16_t Data) {
  Changes.push_back(make_unique<SetGlobalValueSubClassData>(GV, Data, this));
}

void CheckpointEngine::setGlobalValueBitfield(GlobalValue *GV) {
  Changes.push_back(make_unique<GlobalValueBitfield>(GV, this));
}

void CheckpointEngine::setFnAttributes(Function *F) {
  Changes.push_back(make_unique<SetFnAttributes>(F, this));
}

void CheckpointEngine::setCallBaseAttributes(CallBase *C) {
  Changes.push_back(make_unique<SetCallBaseAttributes>(C, this));
}

void CheckpointEngine::setGlobalVariableAttributes(GlobalVariable *GV) {
  Changes.push_back(make_unique<SetGlobalVariableAttributes>(GV, this));
}

void CheckpointEngine::setGlobalVariableInitializer(GlobalVariable *GV) {
  Changes.push_back(make_unique<SetGlobalVariableInitializer>(GV, this));
}

void CheckpointEngine::setGlobalVariableBits(GlobalVariable *GV) {
  Changes.push_back(make_unique<SetGlobalVariableBits>(GV, this));
}

void CheckpointEngine::removeGlobalVariable(GlobalVariable *GV) {
  Changes.push_back(make_unique<RemoveGlobalVariable>(GV, this));
}

void CheckpointEngine::deleteGlobalVariable(GlobalVariable *GV) {
  Changes.push_back(make_unique<DeleteGlobalVariable>(GV, this));
}

void CheckpointEngine::removeGlobalAlias(GlobalAlias *GA) {
  Changes.push_back(make_unique<RemoveGlobalAlias>(GA, this));
}

void CheckpointEngine::deleteGlobalAlias(GlobalAlias *GA) {
  Changes.push_back(make_unique<DeleteGlobalAlias>(GA, this));
}

void CheckpointEngine::insertGlobalAlias(GlobalAlias *GA) {
  Changes.push_back(make_unique<InsertGlobalAlias>(GA, this));
}

void CheckpointEngine::removeGlobalIFunc(GlobalIFunc *GIF) {
  Changes.push_back(make_unique<RemoveGlobalIFunc>(GIF, this));
}

void CheckpointEngine::deleteGlobalIFunc(GlobalIFunc *GIF) {
  Changes.push_back(make_unique<DeleteGlobalIFunc>(GIF, this));
}

void CheckpointEngine::insertGlobalIFunc(GlobalIFunc *GIF) {
  Changes.push_back(make_unique<InsertGlobalIFunc>(GIF, this));
}

void CheckpointEngine::removeNamedMDNode(NamedMDNode *MDNode) {
  Changes.push_back(make_unique<RemoveNamedMDNode>(MDNode, this));
}

void CheckpointEngine::deleteNamedMDNode(NamedMDNode *MDNode) {
  Changes.push_back(make_unique<DeleteNamedMDNode>(MDNode, this));
}

void CheckpointEngine::insertNamedMDNode(NamedMDNode *MDNode) {
  Changes.push_back(make_unique<InsertNamedMDNode>(MDNode, this));
}

void CheckpointEngine::setComdat(GlobalObject *GO) {
  Changes.push_back(make_unique<SetComdat>(GO, this));
}

void CheckpointEngine::setDebugLoc(Instruction *I) {
  Changes.push_back(make_unique<SetDebugLoc>(I, this));
}

void CheckpointEngine::removeBB(BasicBlock *BB) {
  Changes.push_back(make_unique<RemoveBB>(BB, this));
}

void CheckpointEngine::moveBB(BasicBlock *BB) {
  Changes.push_back(make_unique<MoveBB>(BB, this));
}

void CheckpointEngine::spliceBB(Value *OrigInstrOrBB, Instruction *FirstI,
                                Instruction *LastI) {
  Changes.push_back(make_unique<SpliceBB>(OrigInstrOrBB, FirstI, LastI, this));
}

void CheckpointEngine::insertBB(BasicBlock *BB) {
  Changes.push_back(make_unique<InsertBB>(BB, this));
}

void CheckpointEngine::spliceFn(Value *OrigBBOrFn, BasicBlock *FirstBB,
                                BasicBlock *LastBB) {
  Changes.push_back(make_unique<SpliceFn>(OrigBBOrFn, FirstBB, LastBB, this));
}

void CheckpointEngine::removeFn(Function *Fn) {
  Changes.push_back(make_unique<RemoveFn>(Fn, this));
}

void CheckpointEngine::createValue(Value *NewV) {
  Changes.push_back(make_unique<CreateValue>(NewV, this));
}

void CheckpointEngine::deleteValue(Value *DelV) {
  Changes.push_back(make_unique<DeleteValue>(DelV, this));
}

void CheckpointEngine::destroyConstant(Constant *C) {
  Changes.push_back(make_unique<DestroyConstant>(C, this));
}

void CheckpointEngine::clearInstList(BasicBlock *BB) {
  Changes.push_back(make_unique<ClearInstList>(BB, this));
}

#ifndef NDEBUG
void CheckpointEngine::dump(raw_ostream &OS) const {
  for (const auto &ChangePtr : Changes)
    ChangePtr->dump(OS);
  OS << "\n";
}
void CheckpointEngine::dump() const {
  dump(dbgs());
}
#endif // NDEBUG

void CheckpointEngine::accept() {
  Active = false;
  for (auto &ChangePtr : Changes)
    ChangePtr->apply();
  clear();
}

void CheckpointEngine::rollback() {
  assert(Active && "Trying to restore() without having called save()");
  Active = false;
  InRollback = true;
  // Iterate through the changes in reverse and revert them one by one.
  for (auto &ChangePtr : llvm::reverse(Changes))
    ChangePtr->revert();
  InRollback = false;
  clear();
#ifndef NDEBUG
  if (RunVerifier)
    IRChecker.expectNoDiff();
#endif // NDEBUG
}

CheckpointGuard CheckpointEngine::disable() {
  return CheckpointGuard(false, this);
}

void CheckpointEngine::startTracking(bool RunVerifier,
                                     uint32_t MaxNumOfTrackedChanges) {
  clear();
#ifndef NDEBUG
  this->RunVerifier = RunVerifier;
  if (RunVerifier) {
    LLVMContextImpl *ZeroBasedCImpl = (LLVMContextImpl *)0;
    auto *CImpl = (LLVMContextImpl *)(((char *)this) -
                                      (char *)&ZeroBasedCImpl->ChkpntEngine);
    // TODO: For now we only check one module.
    Module *M = *CImpl->OwnedModules.begin();
    IRChecker.save(*M);
  }
#endif // NDEBUG
  Active = true;
  MaxNumChanges = MaxNumOfTrackedChanges;
}
