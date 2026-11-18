//===- RelLookupTableConverterPass - Rel Table Conv -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements relative lookup table converter that converts
// lookup tables to relative lookup tables to make them PIC-friendly.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/RelLookupTableConverter.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

using namespace llvm;

struct LookupTableInfo {
  SmallVector<Constant *> Ptrs;
};

struct LookupTableUseInfo {
  Value *Index;
  APInt Stride;
  Type *ElemType;
  GetElementPtrInst *GEP;
  LoadInst *Load;
};

static bool isEligibleForRelLookupTable(GlobalVariable &GV) {
  // If lookup table has more than one user,
  // do not generate a relative lookup table.
  // This is to simplify the analysis that needs to be done for this pass.
  // TODO: Add support for lookup tables with multiple uses.
  // For ex, this can happen when a function that uses a lookup table gets
  // inlined into multiple call sites.
  //
  // If the original lookup table does not have local linkage and is
  // not dso_local, do not generate a relative lookup table.
  // This optimization creates a relative lookup table that consists of
  // offsets between the start of the lookup table and its elements.
  // To be able to generate these offsets, relative lookup table and
  // its elements should have internal linkage and be dso_local, which means
  // that they should resolve to symbols within the same linkage unit.
  return GV.hasInitializer() && GV.isConstant() && GV.hasOneUse() &&
         GV.hasLocalLinkage() && GV.isDSOLocal() && GV.isImplicitDSOLocal();
}

static bool analyzeLookupTableUse(LookupTableUseInfo &Info, User *Usr) {
  Info.GEP = dyn_cast<GetElementPtrInst>(Usr);
  if (!Info.GEP || !Info.GEP->hasOneUse())
    return false;

  Info.Load = dyn_cast<LoadInst>(Info.GEP->use_begin()->getUser());
  if (!Info.Load || Info.Load->isVolatile())
    return false;

  // If values are not 64-bit pointers, do not generate a relative lookup table.
  const DataLayout &DL = Info.GEP->getDataLayout();
  Info.ElemType = Info.Load->getType();
  if (!Info.ElemType->isPointerTy() ||
      DL.getPointerTypeSizeInBits(Info.ElemType) != 64)
    return false;

  // Make sure this is a gep of the form GV + scale*var.
  unsigned IndexWidth = DL.getIndexTypeSizeInBits(Info.GEP->getType());
  SmallMapVector<Value *, APInt, 4> VarOffsets;
  APInt ConstOffset(IndexWidth, 0);
  if (!Info.GEP->collectOffset(DL, IndexWidth, VarOffsets, ConstOffset) ||
      !ConstOffset.isZero() || VarOffsets.size() != 1)
    return false;

  // This can't be a pointer lookup table if the stride is smaller than a
  // pointer.
  Info.Index = VarOffsets.front().first;
  Info.Stride = VarOffsets.front().second;
  if (Info.Stride.ult(DL.getTypeStoreSize(Info.ElemType)))
    return false;
  return true;
}

static bool canConvertToRelLookupTable(LookupTableInfo &Info,
                                       const LookupTableUseInfo &UseInfo,
                                       GlobalVariable &GV) {
  SmallVector<GlobalVariable *, 4> GVOps;
  Module &M = *GV.getParent();
  const DataLayout &DL = M.getDataLayout();
  Triple TT = M.getTargetTriple();
  // FIXME: This should be removed in the future.
  bool ShouldDropUnnamedAddr =
      // Drop unnamed_addr to avoid matching pattern in
      // `handleIndirectSymViaGOTPCRel`, which generates GOTPCREL relocations
      // not supported by the GNU linker and LLD versions below 18 on aarch64.
      TT.isAArch64()
      // Apple's ld64 (and ld-prime on Xcode 15.2) miscompile something on
      // x86_64-apple-darwin. See
      // https://github.com/rust-lang/rust/issues/140686 and
      // https://github.com/rust-lang/rust/issues/141306.
      || (TT.isX86() && TT.isOSDarwin());

  APInt Offset(UseInfo.Stride.getBitWidth(), 0);
  uint64_t GVSize = GV.getGlobalSize(DL);
  for (; Offset.ult(GVSize); Offset += UseInfo.Stride) {
    Constant *C = ConstantFoldLoadFromConst(GV.getInitializer(),
                                            UseInfo.ElemType, Offset, DL);
    if (!C)
      return false;

    GlobalValue *GVOp;
    APInt GVOffset;

    // If an operand is not a constant offset from a lookup table,
    // do not generate a relative lookup table.
    if (!IsConstantOffsetFromGlobal(C, GVOp, GVOffset, DL))
      return false;

    // If operand is mutable, do not generate a relative lookup table.
    auto *GlobalVarOp = dyn_cast<GlobalVariable>(GVOp);
    if (!GlobalVarOp || !GlobalVarOp->isConstant())
      return false;

    if (!GlobalVarOp->hasLocalLinkage() || !GlobalVarOp->isDSOLocal() ||
        !GlobalVarOp->isImplicitDSOLocal())
      return false;

    // On AArch64 small code model, the text-to-data span can be up to 4GB,
    // which exceeds 32-bit signed relative offsets. Avoid converting if the
    // target operand requires dynamic relocations (placing it in .data.rel.ro
    // in the data segment rather than .rodata in the text segment).
    if (TT.isAArch64() &&
        (!GlobalVarOp->hasInitializer() ||
         GlobalVarOp->getInitializer()->needsDynamicRelocation()))
      return false;

    if (ShouldDropUnnamedAddr)
      GVOps.push_back(GlobalVarOp);

    Info.Ptrs.push_back(C);
  }

  if (ShouldDropUnnamedAddr)
    for (auto *GVOp : GVOps)
      GVOp->setUnnamedAddr(GlobalValue::UnnamedAddr::None);

  return true;
}

static GlobalVariable *createRelLookupTable(const LookupTableInfo &Info,
                                            GlobalVariable &LookupTable) {
  Module &M = *LookupTable.getParent();
  ArrayType *IntArrayTy =
      ArrayType::get(Type::getInt32Ty(M.getContext()), Info.Ptrs.size());

  GlobalVariable *RelLookupTable = new GlobalVariable(
      M, IntArrayTy, LookupTable.isConstant(), LookupTable.getLinkage(),
      nullptr, LookupTable.getName() + ".rel", &LookupTable,
      LookupTable.getThreadLocalMode(), LookupTable.getAddressSpace(),
      LookupTable.isExternallyInitialized());

  Type *IntPtrTy = M.getDataLayout().getIntPtrType(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  Constant *Base = ConstantExpr::getPtrToInt(RelLookupTable, IntPtrTy);

  uint64_t Idx = 0;
  SmallVector<Constant *, 64> RelLookupTableContents(Info.Ptrs.size());

  for (Constant *Element : Info.Ptrs) {
    Constant *Target = ConstantExpr::getPtrToInt(Element, IntPtrTy);
    Constant *Sub = ConstantExpr::getSub(Target, Base);
    Constant *RelOffset = ConstantExpr::getTrunc(Sub, Int32Ty);
    RelLookupTableContents[Idx++] = RelOffset;
  }

  Constant *Initializer =
      ConstantArray::get(IntArrayTy, RelLookupTableContents);
  RelLookupTable->setInitializer(Initializer);
  RelLookupTable->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  RelLookupTable->setAlignment(llvm::Align(4));
  return RelLookupTable;
}

static void convertLookupTableUse(LookupTableUseInfo &Info, Value *Ptr) {
  BasicBlock *BB = Info.GEP->getParent();
  Module *M = BB->getModule();
  IRBuilder<> Builder(BB);

  // Place new instruction sequence before GEP.
  Builder.SetInsertPoint(Info.GEP);
  IntegerType *IntTy = cast<IntegerType>(Info.Index->getType());
  Value *Offset = Builder.CreateShl(Info.Index, ConstantInt::get(IntTy, 2),
                                    "reltable.shift");

  // Insert the call to load.relative intrinsic before LOAD.
  // GEP might not be immediately followed by a LOAD, like it can be hoisted
  // outside the loop or another instruction might be inserted them in between.
  Builder.SetInsertPoint(Info.Load);
  Function *LoadRelIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(
      M, Intrinsic::load_relative, {Info.Index->getType()});

  // Create a call to load.relative intrinsic that computes the target address
  // by adding base address (lookup table address) and relative offset.
  Value *Result = Builder.CreateCall(LoadRelIntrinsic, {Ptr, Offset},
                                     "reltable.intrinsic");

  // Replace load instruction with the new generated instruction sequence.
  Info.Load->replaceAllUsesWith(Result);
  // Remove Load and GEP instructions.
  Info.Load->eraseFromParent();
  Info.GEP->eraseFromParent();
}

// Convert lookup tables to relative lookup tables in the module.
static bool convertToRelativeLookupTables(
    Module &M, function_ref<TargetTransformInfo &(Function &)> GetTTI) {
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    // Check if we have a target that supports relative lookup tables.
    if (!GetTTI(F).shouldBuildRelLookupTables())
      return false;

    // We assume that the result is independent of the checked function.
    break;
  }

  bool Changed = false;

  SmallVector<GlobalVariable *> DeadGVs;
  for (GlobalVariable &GV : llvm::make_early_inc_range(M.globals())) {
    if (!isEligibleForRelLookupTable(GV))
      continue;

    // Try to look through phi/select. In that case, make sure that all the
    // operands are eligible for lookup table conversion and collect them.
    SmallVector<std::pair<GlobalVariable *, LookupTableInfo>> GVs;
    Value *Ptr = &GV;
    User *Usr = *Ptr->user_begin();
    if (isa<PHINode, SelectInst>(Usr)) {
      if (!Usr->hasOneUse())
        continue;

      bool AllEligible = true;
      for (Value *Op : drop_begin(Usr->operands(), isa<SelectInst>(Usr))) {
        auto *OpGV = dyn_cast<GlobalVariable>(Op);
        if (!OpGV || !isEligibleForRelLookupTable(*OpGV)) {
          AllEligible = false;
          break;
        }
        GVs.emplace_back(OpGV, LookupTableInfo());
      }
      if (!AllEligible)
        continue;

      Ptr = Usr;
      Usr = *Ptr->user_begin();
    } else {
      GVs.emplace_back(&GV, LookupTableInfo());
    }

    LookupTableUseInfo UseInfo;
    if (!analyzeLookupTableUse(UseInfo, Usr))
      continue;

    if (!all_of(GVs, [&](auto &Pair) {
          return canConvertToRelLookupTable(Pair.second, UseInfo, *Pair.first);
        }))
      continue;

    convertLookupTableUse(UseInfo, Ptr);

    for (auto &[GV, Info] : GVs) {
      GlobalVariable *RelLookupTable = createRelLookupTable(Info, *GV);
      GV->replaceAllUsesWith(RelLookupTable);
      DeadGVs.push_back(GV);
    }

    Changed = true;
  }

  // Remove the original lookup tables.
  for (GlobalVariable *DeadGV : DeadGVs)
    DeadGV->eraseFromParent();

  return Changed;
}

PreservedAnalyses RelLookupTableConverterPass::run(Module &M,
                                                   ModuleAnalysisManager &AM) {
  FunctionAnalysisManager &FAM =
      AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  auto GetTTI = [&](Function &F) -> TargetTransformInfo & {
    return FAM.getResult<TargetIRAnalysis>(F);
  };

  if (!convertToRelativeLookupTables(M, GetTTI))
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
