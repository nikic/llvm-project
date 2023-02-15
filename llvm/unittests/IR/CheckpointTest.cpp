//===- llvm/unittest/IR/CheckpointTest.cpp - Checkpoint unit tests --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Checkpoint.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Checkpoint.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/Debugify.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"
#include "llvm/Transforms/Utils/SSAUpdaterBulk.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Vectorize/LoopVectorize.h"
#include "gtest/gtest.h"

namespace llvm {
namespace {

static std::unique_ptr<Module> parseIR(LLVMContext &C, const char *IR) {
  SMDiagnostic Err;
  std::unique_ptr<Module> Mod = parseAssemblyString(IR, Err, C);
  if (!Mod)
    Err.print("CheckpointTest", errs());
  return Mod;
}

static BasicBlock *getBBWithName(Function *F, StringRef Name) {
  auto It = find_if(
      *F, [&Name](const BasicBlock &BB) { return BB.getName() == Name; });
  assert(It != F->end() && "Not found!");
  return &*It;
}

TEST(CheckpointTest, HandleOutOfScope) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *Instr = &*std::next(BB0->begin(), 0);

  {
    Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
    Instr->eraseFromParent();
    EXPECT_EQ(BB0->size(), 1u);
  }
  EXPECT_EQ(BB0->size(), 1u);
}

TEST(CheckpointTest, SetNameInstr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *Instr = &*std::next(BB0->begin(), 0);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->setName("new");
  EXPECT_NE(Instr->getName(), "instr");
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getName(), "instr");
}

TEST(CheckpointTest, TakeNameInstr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %instr2 = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *Instr2 = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr1->takeName(Instr2);
  EXPECT_EQ(Instr1->getName(), "instr2");
  EXPECT_FALSE(Instr2->hasName());
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr1->getName(), "instr1");
  EXPECT_EQ(Instr2->getName(), "instr2");
}

TEST(CheckpointTest, SetNameArg) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  Argument *Arg0 = F->getArg(0);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Arg0->setName("ARG0");
  EXPECT_NE(Arg0->getName(), "a");
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Arg0->getName(), "a");
}

TEST(CheckpointTest, SetNameBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  BB0->setName("NEWNAME");
  EXPECT_NE(BB0->getName(), "bb0");
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->getName(), "bb0");
}

TEST(CheckpointTest, SetNameFn) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  F->setName("bar");
  EXPECT_NE(F->getName(), "foo");
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(F->getName(), "foo");
}

TEST(CheckpointTest, CreateInstr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  auto *Add = BinaryOperator::CreateAdd(Arg0, Arg1);
  auto *Ret = BB0->getTerminator();
  Add->insertBefore(Ret);
  EXPECT_NE(BB0->size(), 1u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 1u);
}

TEST(CheckpointTest, CreateInstrNoParent) {
  // Make sure we don't crash if the newly created instruction has no parent.
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  // This won't insert the instruction into a BB.
  auto *Add =
      BinaryOperator::Create(Instruction::BinaryOps::Add, Arg0, Arg1, "Add",
                             /*InsertBefore=*/(Instruction *)nullptr);
  (void)Add;
  Chkpnt.rollback();
}

TEST(CheckpointTest, RemoveInstr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %toRemove = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *ToRemoveI = &*BB0->begin();

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  ToRemoveI->removeFromParent();
  EXPECT_NE(BB0->size(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 2u);
}

TEST(CheckpointTest, EraseInstr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %toDelete = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *ToDeleteI = &*BB0->begin();

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  ToDeleteI->eraseFromParent();
  EXPECT_NE(BB0->size(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 2u);
}

TEST(CheckpointTest, InsertBefore) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = sub i32 %a, %b
  %instr2 = sub i32 %b, %a
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *Instr2 = &*It++;
  auto *Ret = &*It++;
  EXPECT_EQ(BB0->size(), 3u);
  auto *NewI = BinaryOperator::CreateAdd(Arg0, Arg1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  NewI->insertBefore(Instr2);
  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 3u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Ret);

  NewI->deleteValue();
}

TEST(CheckpointTest, InsertAfter) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = sub i32 %a, %b
  %instr2 = sub i32 %b, %a
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *Instr2 = &*It++;
  auto *Ret = &*It++;
  EXPECT_EQ(BB0->size(), 3u);
  auto *NewI = BinaryOperator::CreateAdd(Arg0, Arg1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  NewI->insertAfter(Instr1);
  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 3u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Ret);

  NewI->deleteValue();
}

TEST(CheckpointTest, InsertAt) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = sub i32 %a, %b
  %instr2 = sub i32 %b, %a
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *Instr2 = &*It++;
  auto *Ret = &*It++;
  EXPECT_EQ(BB0->size(), 3u);
  auto *NewI = BinaryOperator::CreateAdd(Arg0, Arg1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  NewI->insertInto(BB0, Instr2->getIterator());
  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 3u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Ret);

  NewI->deleteValue();
}

TEST(CheckpointTest, MetadataSet) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *Instr = &*std::next(BB0->begin(), 0);
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  MDNode *MD1 =
      MDNode::get(C, ArrayRef<Metadata *>(ConstantAsMetadata::get(FortyTwo)));
  MDNode *MD2 =
      MDNode::get(C, ArrayRef<Metadata *>(ConstantAsMetadata::get(FortyTwo)));

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->setMetadata("MD1", MD1);
  Instr->setMetadata("MD2", MD2);
  EXPECT_EQ(Instr->getMetadata("MD1"), MD1);
  EXPECT_EQ(Instr->getMetadata("MD2"), MD2);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_FALSE(Instr->hasMetadata("MD1"));
  EXPECT_FALSE(Instr->hasMetadata("MD2"));
}

TEST(CheckpointTest, MetadataUnset) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b, !MD1 !0
  ret void
}
!0 = !{i32 42}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *Instr = &*std::next(BB0->begin(), 0);
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  MDNode *MD1 =
      MDNode::get(C, ArrayRef<Metadata *>(ConstantAsMetadata::get(FortyTwo)));

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->setMetadata("MD1", nullptr);
  EXPECT_FALSE(Instr->hasMetadata("MD1"));
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getMetadata("MD1"), MD1);
}

TEST(CheckpointTest, MetadataClear) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b, !MD1 !0
  ret void
}
!0 = !{i32 42}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto *Instr = &*std::next(BB0->begin(), 0);
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  MDNode *MD1 =
      MDNode::get(C, ArrayRef<Metadata *>(ConstantAsMetadata::get(FortyTwo)));

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  Instr->dropUnknownNonDebugMetadata();
  EXPECT_FALSE(Instr->hasMetadata("MD1"));
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getMetadata("MD1"), MD1);
}

TEST(CheckpointTest, MetadataAdd) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = external global i32
define void @foo() {
bb0:
  ret void
}
)");
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  MDNode *MD1 =
      MDNode::get(C, ArrayRef<Metadata *>(ConstantAsMetadata::get(FortyTwo)));
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  GV->addMetadata("MD1", *MD1);
  Chkpnt.rollback();
}

TEST(CheckpointTest, MetadataRAUW) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV0 = external global i32
@GV1 = external global i64
define void @foo() {
bb0:
  ret void
}
)");
  GlobalVariable *GV0 = M->getGlobalVariable("GV0");
  GlobalVariable *GV1 = M->getGlobalVariable("GV1");
  ValueAsMetadata *MD = ValueAsMetadata::get(GV0);
  EXPECT_EQ(MD->getValue(), GV0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  GV0->replaceAllUsesWith(GV1);
  EXPECT_EQ(MD->getValue(), GV1);
  Chkpnt.rollback();
  EXPECT_EQ(MD->getValue(), GV0);
}

TEST(CheckpointTest, MetadataRAUWBackToBack) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV0 = external global i32
@GV1 = external global i64
define void @foo() {
bb0:
  ret void
}
)");
  GlobalVariable *GV0 = M->getGlobalVariable("GV0");
  GlobalVariable *GV1 = M->getGlobalVariable("GV1");
  ValueAsMetadata *MD = ValueAsMetadata::get(GV0);
  EXPECT_EQ(MD->getValue(), GV0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  GV0->replaceAllUsesWith(GV1);
  EXPECT_EQ(MD->getValue(), GV1);
  GV1->replaceAllUsesWith(GV0);
  EXPECT_EQ(MD->getValue(), GV0);

  Chkpnt.rollback();
  EXPECT_EQ(MD->getValue(), GV0);
}

TEST(CheckpointTest, MetadataRAUWMD) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV0 = external global i32
@GV1 = external global i64
define void @foo() {
bb0:
  ret void
}
)");
  auto Tmp1 = MDTuple::getTemporary(C, std::nullopt);
  auto Tmp2 = MDTuple::getTemporary(C, std::nullopt);
  auto *N = MDTuple::get(C, {Tmp1.get()});
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(N->operands().begin()->get(), Tmp1.get());
  Tmp1->replaceAllUsesWith(Tmp2.get());
  EXPECT_EQ(N->operands().begin()->get(), Tmp2.get());

  Chkpnt.rollback();
  EXPECT_EQ(N->operands().begin()->get(), Tmp1.get());
}

TEST(CheckpointTest, MetadataRauwMAV1) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  auto Tmp1 = MDTuple::getTemporary(C, std::nullopt);
  auto Tmp2 = MDTuple::getTemporary(C, std::nullopt);
  auto *MAV = MetadataAsValue::get(C, Tmp1.get());
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(MAV->getMetadata(), Tmp1.get());
  Tmp1->replaceAllUsesWith(Tmp2.get());
  EXPECT_EQ(MAV->getMetadata(), Tmp2.get());

  Chkpnt.rollback();
  EXPECT_EQ(MAV->getMetadata(), Tmp1.get());
}

TEST(CheckpointTest, MetadataRauwMAV2) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  auto *F = M->getFunction("foo");
  auto *BB0 = getBBWithName(F, "bb0");
  auto Tmp1 = MDTuple::getTemporary(C, std::nullopt);
  auto Tmp2 = MDTuple::getTemporary(C, std::nullopt);
  auto *MAV1 = MetadataAsValue::get(C, Tmp1.get());
  auto *MAV2 = MetadataAsValue::get(C, Tmp2.get());
  auto *Intrinsic = Function::Create(
      FunctionType::get(Type::getVoidTy(C), Type::getMetadataTy(C), false),
      GlobalValue::ExternalLinkage, "llvm.intrinsic", M.get());
  auto *MAV1User = CallInst::Create(Intrinsic, MAV1, "", BB0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(MAV1->getMetadata(), Tmp1.get());
  EXPECT_EQ(MAV2->getMetadata(), Tmp2.get());
  EXPECT_EQ(MAV1User->getOperand(0), MAV1);
  Tmp1->replaceAllUsesWith(Tmp2.get());
  EXPECT_EQ(MAV1->getMetadata(), nullptr);
  EXPECT_EQ(MAV2->getMetadata(), Tmp2.get());
  EXPECT_EQ(MAV1User->getOperand(0), MAV2);

  Chkpnt.rollback();
  EXPECT_EQ(MAV1->getMetadata(), Tmp1.get());
  EXPECT_EQ(MAV2->getMetadata(), Tmp2.get());
  EXPECT_EQ(MAV1User->getOperand(0), MAV1);
}

TEST(CheckpointTest, MetadataRauwNull) {
  LLVMContext C;
  auto M = std::make_unique<Module>("M", C);
  auto Tmp1 = MDTuple::getTemporary(C, std::nullopt);
  auto *MAV1 = MetadataAsValue::get(C, Tmp1.get());

  Checkpoint Chkpnt = C.getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(MAV1->getMetadata(), Tmp1.get());
  Tmp1->replaceAllUsesWith(nullptr);
  Chkpnt.rollback();
  EXPECT_EQ(MAV1->getMetadata(), Tmp1.get());
}

TEST(CheckpointTest, MetadataWeights) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i1 %cond0) {
entry:
  br i1 %cond0, label %bb0, label %bb1, !prof !1
bb0:
 %0 = mul i32 1, 2
 br label %bb1
bb1:
  ret void
}

!1 = !{!"branch_weights", i32 1, i32 100000}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Function *F = M->getFunction("foo");
  BasicBlock *BB = getBBWithName(F, "entry");
  auto *Branch = cast<BranchInst>(&*BB->begin());
  Branch->setMetadata(LLVMContext::MD_prof, nullptr);
  EXPECT_EQ(Branch->getMetadata(LLVMContext::MD_prof), nullptr);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, MoveBefore) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr0 = add i32 %a, %b
  %instr1 = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Instr0 = &*It++;
  auto *Instr1 = &*It++;
  auto *Ret = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr1->moveBefore(Instr0);
  //bb0:
  //  %instr1 = add i32 %a, %b
  //  %instr0 = add i32 %a, %b
  //  ret void
  EXPECT_NE(&*std::next(BB0->begin(), 0), Instr0);
  EXPECT_NE(&*std::next(BB0->begin(), 1), Instr1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(&*std::next(BB0->begin(), 0), Instr0);
  EXPECT_EQ(&*std::next(BB0->begin(), 1), Instr1);
  EXPECT_EQ(&*std::next(BB0->begin(), 2), Ret);
}

TEST(CheckpointTest, MoveAfter) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr0 = add i32 %a, %b
  %instr1 = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Instr0 = &*It++;
  auto *Instr1 = &*It++;
  auto *Ret = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr0->moveAfter(Instr1);
  //bb0:
  //  %instr1 = add i32 %a, %b
  //  %instr0 = add i32 %a, %b
  //  ret void
  EXPECT_NE(&*std::next(BB0->begin(), 0), Instr0);
  EXPECT_NE(&*std::next(BB0->begin(), 1), Instr1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(&*std::next(BB0->begin(), 0), Instr0);
  EXPECT_EQ(&*std::next(BB0->begin(), 1), Instr1);
  EXPECT_EQ(&*std::next(BB0->begin(), 2), Ret);
}

TEST(CheckpointTest, SetDebugLoc) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %dbgInstr = sub i32 %a, %b, !dbg !2
  ret void
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1)
!1 = !DIFile(filename: "foo.ll", directory: "/")
!2 = !DILocation(line: 1, column: 1, scope: !4)
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "f", linkageName: "f", scope: null, file: !1, line: 1, scopeLine: 1, unit: !0)
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *DbgInstr = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr1->setDebugLoc(DbgInstr->getDebugLoc());
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_NE(Instr1->getDebugLoc(), DbgInstr->getDebugLoc());
}

TEST(CheckpointTest, SetOperand) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->setOperand(0, Arg1);
  Instr->setOperand(1, Arg0);
  EXPECT_NE(Instr->getOperand(0), Arg0);
  EXPECT_NE(Instr->getOperand(1), Arg1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getOperand(0), Arg0);
  EXPECT_EQ(Instr->getOperand(1), Arg1);
}

TEST(CheckpointTest, SetOperandConstant) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
declare void @Bar()
@GV = global ptr @Foo
define void @F() {
bb0:
  ret void
}
)");
  Function *Foo = M->getFunction("Foo");
  Function *Bar = M->getFunction("Bar");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  EXPECT_EQ(GV->getOperand(0), Foo);
  Chkpnt.save();

  Foo->replaceAllUsesWith(Bar);
  EXPECT_EQ(GV->getOperand(0), Bar);
  Chkpnt.rollback();
  EXPECT_EQ(GV->getOperand(0), Foo);
}

TEST(CheckpointTest, GlobalVariableNew) {
  LLVMContext C;
  auto M = std::make_unique<Module>("M", C);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  GlobalVariable *GV = new GlobalVariable(
      Type::getInt32Ty(C), /*isConstant=*/true, GlobalValue::InternalLinkage);
  (void)GV;
  Chkpnt.rollback();
}

TEST(CheckpointTest, GlobalVariableEraseFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GV_Before = external global i32
@GV = global ptr @Foo
@GV_After = external global i64
)");
  Function *Foo = M->getFunction("Foo");
  GlobalVariable *GV_Before = M->getGlobalVariable("GV_Before");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  GlobalVariable *GV_After = M->getGlobalVariable("GV_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(M->global_size(), 3u);
  GV->eraseFromParent();
  EXPECT_EQ(M->global_size(), 2u);
  EXPECT_EQ(GV->getOperand(0), nullptr);
  Chkpnt.rollback();
  EXPECT_EQ(M->global_size(), 3u);
  EXPECT_EQ(GV->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GV->getIterator()), GV_Before);
  EXPECT_EQ(&*std::next(GV->getIterator()), GV_After);

  // Erase the first in the list.
  Chkpnt.save();
  GV_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GV_Before->getIterator()), GV);

  // Erase the last in the list.
  Chkpnt.save();
  GV_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GV_After->getIterator()), GV);
}

TEST(CheckpointTest, GlobalVariableEraseFromParentMetadata) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = global i32 42, !type !0
!0 = !{i32 42}
)");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  Chkpnt.save();

  GV->eraseFromParent();
  EXPECT_EQ(GV->getOperand(0), nullptr);
  Chkpnt.rollback();
  EXPECT_EQ(GV->getOperand(0), FortyTwo);
}

TEST(CheckpointTest, GlobalVariableRemoveFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GV_Before = external global i32
@GV = global ptr @Foo
@GV_After = external global i64
)");
  Function *Foo = M->getFunction("Foo");
  GlobalVariable *GV_Before = M->getGlobalVariable("GV_Before");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  GlobalVariable *GV_After = M->getGlobalVariable("GV_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(M->global_size(), 3u);
  GV->removeFromParent();
  EXPECT_EQ(M->global_size(), 2u);
  EXPECT_EQ(GV->getOperand(0), Foo);
  Chkpnt.rollback();
  EXPECT_EQ(M->global_size(), 3u);
  EXPECT_EQ(GV->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GV->getIterator()), GV_Before);
  EXPECT_EQ(&*std::next(GV->getIterator()), GV_After);

  // Try removing the first in the list.
  Chkpnt.save();
  GV_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GV_Before->getIterator()), GV);

  // Try removing the last in the list.
  Chkpnt.save();
  GV_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GV_After->getIterator()), GV);
}

TEST(CheckpointTest, GlobalAliasRemoveFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GA_Before = alias void (), ptr @Foo
@GA = alias void (), ptr @Foo
@GA_After = alias void (), ptr @Foo
define void @F() {
bb0:
  ret void
}
)");
  Function *Foo = M->getFunction("Foo");
  GlobalAlias *GA_Before = M->getNamedAlias("GA_Before");
  GlobalAlias *GA = M->getNamedAlias("GA");
  GlobalAlias *GA_After = M->getNamedAlias("GA_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(M->alias_size(), 3u);
  EXPECT_EQ(GA->getOperand(0), Foo);
  GA->removeFromParent();
  EXPECT_EQ(M->alias_size(), 2u);
  EXPECT_EQ(GA->getOperand(0), Foo);

  Chkpnt.rollback();
  EXPECT_EQ(M->alias_size(), 3u);
  EXPECT_EQ(GA->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GA->getIterator()), GA_Before);
  EXPECT_EQ(&*std::next(GA->getIterator()), GA_After);

  // Remove first in the list.
  Chkpnt.save();
  GA_Before->removeFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GA_Before->getIterator()), GA);

  // Remove last in the list.
  Chkpnt.save();
  GA_After->removeFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GA_After->getIterator()), GA);
}

TEST(CheckpointTest, GlobalAliasEraseFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GA_Before = alias void (), ptr @Foo
@GA = alias void (), ptr @Foo
@GA_After = alias void (), ptr @Foo
)");
  Function *Foo = M->getFunction("Foo");
  GlobalAlias *GA_Before = M->getNamedAlias("GA_Before");
  GlobalAlias *GA = M->getNamedAlias("GA");
  GlobalAlias *GA_After = M->getNamedAlias("GA_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(M->alias_size(), 3u);
  EXPECT_EQ(GA->getOperand(0), Foo);
  GA->eraseFromParent();
  EXPECT_EQ(M->alias_size(), 2u);
  EXPECT_EQ(GA->getOperand(0), nullptr);

  Chkpnt.rollback();
  EXPECT_EQ(M->alias_size(), 3u);
  EXPECT_EQ(GA->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GA->getIterator()), GA_Before);
  EXPECT_EQ(&*std::next(GA->getIterator()), GA_After);

  // Remove first in the list.
  Chkpnt.save();
  GA_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GA_Before->getIterator()), GA);

  // Remove last in the list.
  Chkpnt.save();
  GA_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GA_After->getIterator()), GA);
}

TEST(CheckpointTest, GlobalAliasInsert) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GA_Before = alias void (), ptr @Foo
@GA_After = alias void (), ptr @Foo
)");
  Function *Foo = M->getFunction("Foo");
  GlobalAlias *GA_Before = M->getNamedAlias("GA_Before");
  GlobalAlias *GA_After = M->getNamedAlias("GA_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  EXPECT_EQ(M->alias_size(), 2u);
  auto *NewGA =
      GlobalAlias::create(Foo->getType(), 0, GlobalValue::ExternalLinkage, "NewGA",
                          Foo, /*Parent=*/nullptr);
  Chkpnt.save();
  M->insertAlias(NewGA);
  Chkpnt.rollback();
  EXPECT_EQ(M->alias_size(), 2u);
  EXPECT_EQ(&*std::next(GA_Before->getIterator()), GA_After);
  delete NewGA;
}

TEST(CheckpointTest, GlobalIFuncRemoveFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GIF_Before = ifunc void (), ptr @Foo
@GIF = ifunc void (), ptr @Foo
@GIF_After = ifunc void (), ptr @Foo
)");
  Function *Foo = M->getFunction("Foo");
  GlobalIFunc *GIF_Before = M->getNamedIFunc("GIF_Before");
  GlobalIFunc *GIF = M->getNamedIFunc("GIF");
  GlobalIFunc *GIF_After = M->getNamedIFunc("GIF_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(GIF->getOperand(0), Foo);
  GIF->removeFromParent();
  EXPECT_EQ(GIF->getOperand(0), Foo);

  Chkpnt.rollback();
  EXPECT_EQ(GIF->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GIF->getIterator()), GIF_Before);
  EXPECT_EQ(&*std::next(GIF->getIterator()), GIF_After);

  // Remove first in the list.
  Chkpnt.save();
  GIF_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GIF_Before->getIterator()), GIF);

  // Remove last in the list.
  Chkpnt.save();
  GIF_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GIF_After->getIterator()), GIF);
}

TEST(CheckpointTest, GlobalIFuncEraseFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GIF_Before = ifunc void (), ptr @Foo
@GIF = ifunc void (), ptr @Foo
@GIF_After = ifunc void (), ptr @Foo
)");
  Function *Foo = M->getFunction("Foo");
  GlobalIFunc *GIF_Before = M->getNamedIFunc("GIF_Before");
  GlobalIFunc *GIF = M->getNamedIFunc("GIF");
  GlobalIFunc *GIF_After = M->getNamedIFunc("GIF_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EXPECT_EQ(GIF->getOperand(0), Foo);
  GIF->eraseFromParent();
  EXPECT_EQ(GIF->getOperand(0), nullptr);

  Chkpnt.rollback();
  EXPECT_EQ(GIF->getOperand(0), Foo);
  EXPECT_EQ(&*std::prev(GIF->getIterator()), GIF_Before);
  EXPECT_EQ(&*std::next(GIF->getIterator()), GIF_After);

  // Remove first in the list.
  Chkpnt.save();
  GIF_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(GIF_Before->getIterator()), GIF);

  // Remove last in the list.
  Chkpnt.save();
  GIF_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(GIF_After->getIterator()), GIF);
}

TEST(CheckpointTest, GlobalIFuncInsert) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @Foo()
@GIF_Before = ifunc void (), ptr @Foo
@GIF_After = ifunc void (), ptr @Foo
)");
  Function *Foo = M->getFunction("Foo");
  GlobalIFunc *GIF_Before = M->getNamedIFunc("GIF_Before");
  GlobalIFunc *GIF_After = M->getNamedIFunc("GIF_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  EXPECT_EQ(M->ifunc_size(), 2u);

  Chkpnt.save();
  auto *NewGIF =
      GlobalIFunc::create(Foo->getType(), 0, GlobalValue::ExternalLinkage,
                          "NewGIF", /*Resolver=*/nullptr, /*Parent=*/nullptr);
  M->insertIFunc(NewGIF);
  Chkpnt.rollback();
  EXPECT_EQ(M->ifunc_size(), 2u);
  EXPECT_EQ(&*std::next(GIF_Before->getIterator()), GIF_After);
  delete NewGIF;
}

TEST(CheckpointTest, NamedMDNodeRemoveFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
!MDN_Before = !{}
!MDN = !{}
!MDN_After = !{}
)");
  NamedMDNode *MDN_Before = M->getOrInsertNamedMetadata("MDN_Before");
  NamedMDNode *MDN = M->getOrInsertNamedMetadata("MDN");
  NamedMDNode *MDN_After = M->getOrInsertNamedMetadata("MDN_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  EXPECT_EQ(M->named_metadata_size(), 3u);

  Chkpnt.save();
  M->removeNamedMDNode(MDN);
  EXPECT_EQ(M->named_metadata_size(), 2u);
  Chkpnt.rollback();
  EXPECT_EQ(M->named_metadata_size(), 3u);
  EXPECT_EQ(&*std::prev(MDN->getIterator()), MDN_Before);
  EXPECT_EQ(&*std::next(MDN->getIterator()), MDN_After);

  // Remove first in the list.
  Chkpnt.save();
  M->removeNamedMDNode(MDN_Before);
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(MDN_Before->getIterator()), MDN);

  // Remove last in the list.
  Chkpnt.save();
  M->removeNamedMDNode(MDN_After);
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(MDN_After->getIterator()), MDN);
}

TEST(CheckpointTest, NamedMDNodeEraseFromParent) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
!MDN_Before = !{}
!MDN = !{}
!MDN_After = !{}
)");
  NamedMDNode *MDN_Before = M->getOrInsertNamedMetadata("MDN_Before");
  NamedMDNode *MDN = M->getOrInsertNamedMetadata("MDN");
  NamedMDNode *MDN_After = M->getOrInsertNamedMetadata("MDN_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  Chkpnt.save();
  MDN->eraseFromParent();
  EXPECT_EQ(M->named_metadata_size(), 2u);
  Chkpnt.rollback();
  EXPECT_EQ(M->named_metadata_size(), 3u);
  EXPECT_EQ(&*std::prev(MDN->getIterator()), MDN_Before);
  EXPECT_EQ(&*std::next(MDN->getIterator()), MDN_After);

  // Remove first in the list.
  Chkpnt.save();
  MDN_Before->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::next(MDN_Before->getIterator()), MDN);

  // Remove last in the list.
  Chkpnt.save();
  MDN_After->eraseFromParent();
  Chkpnt.rollback();
  EXPECT_EQ(&*std::prev(MDN_After->getIterator()), MDN);
}

TEST(CheckpointTest, NamedMDNodeInsert) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
!MDN_Before = !{}
!MDN_After = !{}
)");
  NamedMDNode *MDN_Before = M->getOrInsertNamedMetadata("MDN_Before");
  NamedMDNode *MDN_After = M->getOrInsertNamedMetadata("MDN_After");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  EXPECT_EQ(M->named_metadata_size(), 2u);

  Chkpnt.save();
  NamedMDNode *NewMDN = M->getOrInsertNamedMetadata("NewMDN");
  (void)NewMDN;
  Chkpnt.rollback();
  EXPECT_EQ(M->named_metadata_size(), 2u);
  EXPECT_EQ(&*std::next(MDN_Before->getIterator()), MDN_After);
}

TEST(CheckpointTest, GlobalVariableBitfields) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = global i32 42
)");
  auto *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  EXPECT_FALSE(GV->isConstant());
  GV->setConstant(true);
  Chkpnt.rollback();
  EXPECT_FALSE(GV->isConstant());

  EXPECT_FALSE(GV->isExternallyInitialized());
  Chkpnt.save();
  GV->setExternallyInitialized(true);
  EXPECT_TRUE(GV->isExternallyInitialized());
  Chkpnt.rollback();
  EXPECT_FALSE(GV->isExternallyInitialized());
}

TEST(CheckpointTest, SetComdat) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
$C1 = comdat any
$C2 = comdat any
define void @F() comdat($C1) {
bb0:
  ret void
}
)");
  Function *F = M->getFunction("F");
  Comdat *C1 = M->getOrInsertComdat("C1");
  Comdat *C2 = M->getOrInsertComdat("C2");
  EXPECT_EQ(F->getComdat(), C1);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  F->setComdat(nullptr);
  EXPECT_EQ(F->getComdat(), nullptr);
  EXPECT_TRUE(C1->getUsers().empty());

  Chkpnt.rollback();
  EXPECT_EQ(F->getComdat(), C1);

  Chkpnt.save();
  F->setComdat(C2);
  EXPECT_EQ(F->getComdat(), C2);
  EXPECT_TRUE(C1->getUsers().empty());

  Chkpnt.rollback();
  EXPECT_EQ(F->getComdat(), C1);
}

TEST(CheckpointTest, SetShuffleMask) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(<2 x i32> %v1, <2 x i32> %v2) {
bb0:
  %shuffle = shufflevector <2 x i32> %v1, <2 x i32> %v2, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Shuffle = cast<ShuffleVectorInst>(&*It++);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  SmallVector<int> OrigMask(Shuffle->getShuffleMask());
  SmallVector<int> NewMask{42, 42, 42, 42};
  Constant *OrigMaskConst = Shuffle->getShuffleMaskForBitcode();
  Shuffle->setShuffleMask(NewMask);
  EXPECT_EQ(Shuffle->getShuffleMask(), ArrayRef<int>(NewMask));
  EXPECT_NE(Shuffle->getShuffleMaskForBitcode(), OrigMaskConst);
  Chkpnt.rollback();
  EXPECT_EQ(Shuffle->getShuffleMask(), ArrayRef<int>(OrigMask));
  EXPECT_EQ(Shuffle->getShuffleMaskForBitcode(), OrigMaskConst);
}

TEST(CheckpointTest, SwapUse) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b, float %fa, float %fb, <2 x i32> %v1, <2 x i32> %v2) {
bb0:
  %binop = add i32 %a, %b
  %icmp = icmp ult i32 %a, %b
  %fcmp = fcmp ogt float %fa, %fb
  %shuffle = shufflevector <2 x i32> %v1, <2 x i32> %v2, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  br i1 %icmp, label %bb1, label %bb2

bb1:
  ret void
bb2:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *BinOp = cast<BinaryOperator>(&*It++);
  auto *ICmp = cast<ICmpInst>(&*It++);
  auto *FCmp = cast<FCmpInst>(&*It++);
  auto *Shuffle = cast<ShuffleVectorInst>(&*It++);
  auto *Br = cast<BranchInst>(&*It++);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Value *Op0 = BinOp->getOperand(0);
  Value *Op1 = BinOp->getOperand(1);
  BinOp->swapOperands();
  EXPECT_EQ(BinOp->getOperand(0), Op1);
  EXPECT_EQ(BinOp->getOperand(1), Op0);
  Chkpnt.rollback();
  EXPECT_EQ(BinOp->getOperand(0), Op0);
  EXPECT_EQ(BinOp->getOperand(1), Op1);

  Chkpnt.save();
  Op0 = ICmp->getOperand(0);
  Op1 = ICmp->getOperand(1);
  ICmp->swapOperands();
  EXPECT_EQ(ICmp->getOperand(0), Op1);
  EXPECT_EQ(ICmp->getOperand(1), Op0);
  Chkpnt.rollback();
  EXPECT_EQ(ICmp->getOperand(0), Op0);
  EXPECT_EQ(ICmp->getOperand(1), Op1);

  Chkpnt.save();
  Op0 = FCmp->getOperand(0);
  Op1 = FCmp->getOperand(1);
  FCmp->swapOperands();
  EXPECT_EQ(FCmp->getOperand(0), Op1);
  EXPECT_EQ(FCmp->getOperand(1), Op0);
  Chkpnt.rollback();
  EXPECT_EQ(FCmp->getOperand(0), Op0);
  EXPECT_EQ(FCmp->getOperand(1), Op1);

  Chkpnt.save();
  BasicBlock *BB1 = Br->getSuccessor(0);
  BasicBlock *BB2 = Br->getSuccessor(1);
  Br->swapSuccessors();
  EXPECT_EQ(Br->getSuccessor(0), BB2);
  EXPECT_EQ(Br->getSuccessor(1), BB1);
  Chkpnt.rollback();
  EXPECT_EQ(Br->getSuccessor(0), BB1);
  EXPECT_EQ(Br->getSuccessor(1), BB2);

  Chkpnt.save();
  Op0 = Shuffle->getOperand(0);
  Op1 = Shuffle->getOperand(1);
  Shuffle->commute();
  EXPECT_EQ(Shuffle->getOperand(0), Op1);
  EXPECT_EQ(Shuffle->getOperand(1), Op0);
  Chkpnt.rollback();
  EXPECT_EQ(Shuffle->getOperand(0), Op0);
  EXPECT_EQ(Shuffle->getOperand(1), Op1);
}

TEST(CheckpointTest, ConstantVector) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  SmallVector<Constant*, 2> ConstVec;
  auto *C0 = ConstantInt::get(Type::getInt32Ty(C), 42);
  auto *C1 = UndefValue::get(Type::getInt32Ty(C));
  ConstVec.push_back(C0);
  ConstVec.push_back(C1);
  auto *CVec = ConstantVector::get(ConstVec);

  Chkpnt.rollback();
  // Constants are not freed by rollback().
  EXPECT_EQ(CVec->getOperand(0), nullptr);
  EXPECT_EQ(CVec->getOperand(1), nullptr);
}

TEST(CheckpointTest, ConstantArray) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  SmallVector<Constant*, 2> ConstVec;
  Type *ElmTy = Type::getInt32Ty(C);
  auto *C0 = ConstantInt::get(ElmTy, 42);
  auto *C1 = UndefValue::get(ElmTy);
  ConstVec.push_back(C0);
  ConstVec.push_back(C1);
  ArrayType *T = ArrayType::get(ElmTy, 2);
  auto *CArray = ConstantArray::get(T, ConstVec);

  Chkpnt.rollback();
  // Constants are not freed by rollback().
  EXPECT_EQ(CArray->getOperand(0), nullptr);
  EXPECT_EQ(CArray->getOperand(1), nullptr);
}

TEST(CheckpointTest, ConstantHandleOperandChange) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV1 = global i32 42
@GV2 = global i64 43
declare void @B()
define void @F() {
bb0:
  ret void
bb1:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Function *F = M->getFunction("F");
  Function *B = M->getFunction("B");
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  GlobalValue *GV1 = M->getGlobalVariable("GV1");
  GlobalValue *GV2 = M->getGlobalVariable("GV2");
  Type *ElmTy = F->getType();
  SmallVector<Constant*, 1> ConstVec;
  ConstVec.push_back(F);
  ArrayType *Ty = ArrayType::get(ElmTy, 1);
  {
    // ConstantArray
    auto *CArray = ConstantArray::get(Ty, ConstVec);
    Chkpnt.save();
    auto *NewF = Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                                  GlobalValue::ExternalLinkage, "NewF", *M);
    EXPECT_EQ(CArray->getOperand(0), F);
    CArray->handleOperandChange(F, NewF);
    EXPECT_EQ(CArray->getOperand(0), NewF);
    Chkpnt.rollback();
    EXPECT_EQ(CArray->getOperand(0), F);
  }

  {
    // ConstantVector
    auto *CVec = ConstantVector::get(ConstVec);
    Chkpnt.save();
    auto *NewF = Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                                  GlobalValue::ExternalLinkage, "NewF", *M);
    EXPECT_EQ(CVec->getOperand(0), F);
    CVec->handleOperandChange(F, NewF);
    EXPECT_EQ(CVec->getOperand(0), NewF);
    Chkpnt.rollback();
    EXPECT_EQ(CVec->getOperand(0), F);
  }

  {
    // ConstantStruct
    SmallVector<Type *, 1> ElmTypes;
    ElmTypes.push_back(ElmTy);
    StructType *Ty = StructType::create(C, ElmTypes);
    auto *CStruct = ConstantStruct::get(Ty, ConstVec);
    Chkpnt.save();
    auto *NewF = Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                                  GlobalValue::ExternalLinkage, "NewF", *M);
    EXPECT_EQ(CStruct->getOperand(0), F);
    CStruct->handleOperandChange(F, NewF);
    EXPECT_EQ(CStruct->getOperand(0), NewF);
    Chkpnt.rollback();
    EXPECT_EQ(CStruct->getOperand(0), F);
  }

  {
    // ConstantExpr
    auto *CExpr = ConstantExpr::getPtrToInt(F, Type::getInt64Ty(C));
    Chkpnt.save();
    auto *NewF = Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                                  GlobalValue::ExternalLinkage, "NewF", *M);
    CExpr->handleOperandChange(F, NewF);
    EXPECT_EQ(CExpr->getOperand(0), NewF);
    Chkpnt.rollback();
    EXPECT_EQ(CExpr->getOperand(0), F);
  }

  {
    // BlockAddress
    auto *BA = BlockAddress::get(BB0);
    EXPECT_EQ(BA->getBasicBlock(), BB0);
    Chkpnt.save();
    BA->handleOperandChange(BB0, BB1);
    EXPECT_EQ(BA->getBasicBlock(), BB1);
    Chkpnt.rollback();
    EXPECT_EQ(BA->getBasicBlock(), BB0);
  }

  {
    // DSOLocalEquivalent
    auto *DSO = DSOLocalEquivalent::get(F);
    EXPECT_EQ(DSO->getGlobalValue(), F);
    Chkpnt.save();
    DSO->handleOperandChange(F, B);
    EXPECT_EQ(DSO->getGlobalValue(), B);
    Chkpnt.rollback();
    EXPECT_EQ(DSO->getGlobalValue(), F);
  }

  {
    // NoCFIValue
    auto *NoCFI = NoCFIValue::get(GV1);
    EXPECT_EQ(NoCFI->getGlobalValue(), GV1);
    Chkpnt.save();
    NoCFI->handleOperandChange(GV1, GV2);
    Chkpnt.rollback();
    EXPECT_EQ(NoCFI->getGlobalValue(), GV1);
  }
}

TEST(CheckpointTest, NoCFIValueMap) {
  // Checks that the map GV->NoCFIValue is properly maintained.
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV1 = global i32 42
@GV2 = global i32 43
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  GlobalValue *GV1 = M->getGlobalVariable("GV1");
  GlobalValue *GV2 = M->getGlobalVariable("GV2");
  auto *NoCFI = NoCFIValue::get(GV1);
  EXPECT_EQ(NoCFIValue::get(GV1), NoCFI);
  EXPECT_EQ(NoCFI->getGlobalValue(), GV1);
  Chkpnt.save();
  NoCFI->destroyConstant();
  Chkpnt.rollback();
  EXPECT_EQ(NoCFI->getGlobalValue(), GV1);
  EXPECT_EQ(NoCFIValue::get(GV1), NoCFI);

  Chkpnt.save();
  // This erases GV1 from the map
  NoCFI->handleOperandChange(GV1, GV2);
  EXPECT_EQ(NoCFI->getGlobalValue(), GV2);
  Chkpnt.rollback();
  EXPECT_EQ(NoCFI->getGlobalValue(), GV1);
  EXPECT_EQ(NoCFIValue::get(GV1), NoCFI);
}

TEST(CheckpointTest, DSOLocalEquivalentsMap) {
  // Checks that the map GV->DSOLocalEquivalents is properly maintained.
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @F()
declare void @B()
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Function *F = M->getFunction("F");
  Function *B = M->getFunction("B");

  auto *DSO = DSOLocalEquivalent::get(F);
  EXPECT_EQ(DSO->getGlobalValue(), F);
  Chkpnt.save();
  DSO->destroyConstant();
  Chkpnt.rollback();
  EXPECT_EQ(DSOLocalEquivalent::get(F), DSO);

  Chkpnt.save();
  // This erases `DSO` from the map.
  DSO->handleOperandChange(F, B);
  EXPECT_EQ(DSO->getGlobalValue(), B);
  Chkpnt.rollback();
  EXPECT_EQ(DSOLocalEquivalent::get(F), DSO);
}

TEST(CheckpointTest, BlockAddressMap) {
  // Checks that the map GV->BlockAddress is properly maintained.
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @F() {
bb0:
  ret void
bb1:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Function *F = M->getFunction("F");
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");

  auto *BA = BlockAddress::get(BB0);
  EXPECT_EQ(BA->getBasicBlock(), BB0);
  Chkpnt.save();
  BA->destroyConstant();
  Chkpnt.rollback();
  EXPECT_EQ(BlockAddress::get(BB0), BA);

  Chkpnt.save();
  // This erases `BA` from the map.
  BA->handleOperandChange(BB0, BB1);
  EXPECT_EQ(BA->getBasicBlock(), BB1);
  Chkpnt.rollback();
  EXPECT_EQ(BlockAddress::get(BB0), BA);
}

TEST(CheckpointTest, DestroyConstantConstantArray) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @B()
declare void @F()
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Function *F = M->getFunction("F");
  Function *B = M->getFunction("B");
  Type *ElmTy = F->getType();
  SmallVector<Constant*, 2> ConstVec;
  ConstVec.push_back(F);
  ConstVec.push_back(B);
  ArrayType *Ty = ArrayType::get(ElmTy, 2);
  auto *CArray = ConstantArray::get(Ty, ConstVec);
  CArray = ConstantArray::get(Ty, ConstVec);
  Chkpnt.save();
  // This modifies pImpl->ArrayConstants.
  CArray->handleOperandChange(F, B);
  Chkpnt.rollback();
  // Calls ArrayConstants.remove(this) which expects `this` to be in
  // ArrayConstants. So this crashes unless the entry is found in the map.
  CArray->destroyConstant();
}


TEST(CheckpointTest, ConstantHandleOperandChangeSameOperand) {
  // Replacing @bar with null in: [3 x ptr] [ptr null, ptr @F, ptr @B]
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
declare void @F()
declare void @B()
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Function *F = M->getFunction("F");
  Function *B = M->getFunction("B");
  Type *ElmTy = F->getType();
  Constant *Null = ConstantPointerNull::get(PointerType::get(ElmTy, 0));
  SmallVector<Constant*, 3> ConstVec;
  ConstVec.push_back(Null);
  ConstVec.push_back(F);
  ConstVec.push_back(B);
  ArrayType *Ty = ArrayType::get(ElmTy, 3);
  auto *CArray = ConstantArray::get(Ty, ConstVec);
  Chkpnt.save();
  CArray->handleOperandChange(B, Null);
  Chkpnt.rollback();
  EXPECT_EQ(CArray->getOperand(0), Null);
}

TEST(CheckpointTest, ConstantStruct) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  SmallVector<Constant*, 2> ConstVec;
  Type *ElmTy = Type::getInt32Ty(C);
  auto *C0 = ConstantInt::get(ElmTy, 42);
  auto *C1 = UndefValue::get(ElmTy);
  ConstVec.push_back(C0);
  ConstVec.push_back(C1);
  SmallVector<Type *, 2> ElmTypes;
  ElmTypes.push_back(ElmTy);
  ElmTypes.push_back(ElmTy);
  StructType *Ty = StructType::create(C, ElmTypes);
  auto *CStruct = ConstantStruct::get(Ty, ConstVec);

  Chkpnt.rollback();
  EXPECT_EQ(CStruct->getOperand(0), nullptr);
  EXPECT_EQ(CStruct->getOperand(1), nullptr);
}

TEST(CheckpointTest, ConstantExpr) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@addr1 = external global i32
define void @foo() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  Type *Int64Ty = Type::getInt64Ty(C);
  GlobalVariable *Addr1 = M->getGlobalVariable("addr1");

  auto *CE = ConstantExpr::getPtrToInt(Addr1, Int64Ty);
  (void)CE;

  // This used to cause a crash during the destruction of the Module. The reason
  // was that the Constantexpr subclassdata was being reverted, causing the
  // constant not to be found in the Map.
  Chkpnt.rollback();
}

TEST(CheckpointTest, SubclassOptionalData) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b, float %fa, float %fb, ptr %ptr) {
bb0:
  %add = add i32 %a, %b
  %sdiv = sdiv i32 %a, %b
  %fadd = fadd float %fa, %fb
  %gep = getelementptr i32, ptr %ptr, i64 42
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  auto It = BB0->begin();
  auto *Add = &*It++;
  auto *SDiv = &*It++;
  auto *FAdd = &*It++;
  auto *GEP = cast<GetElementPtrInst>(&*It++);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Check clearSubclassOptionalData()
  Add->setHasNoUnsignedWrap();
  Chkpnt.save();
  Add->clearSubclassOptionalData();
  Chkpnt.rollback();
  Add->clearSubclassOptionalData();

  // From this point on we are checking the flags individually.
  Chkpnt.save();
  Add->setHasNoUnsignedWrap(true);
  EXPECT_TRUE(Add->hasNoUnsignedWrap());
  Chkpnt.rollback();
  EXPECT_FALSE(Add->hasNoUnsignedWrap());

  Chkpnt.save();
  Add->setHasNoSignedWrap(true);
  EXPECT_TRUE(Add->hasNoSignedWrap());
  Chkpnt.rollback();
  EXPECT_FALSE(Add->hasNoSignedWrap());

  Chkpnt.save();
  SDiv->setIsExact(true);
  EXPECT_TRUE(SDiv->isExact());
  Chkpnt.rollback();
  EXPECT_FALSE(SDiv->isExact());

  Chkpnt.save();
  FAdd->setFast(true);
  EXPECT_TRUE(FAdd->isFast());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->isFast());

  Chkpnt.save();
  FAdd->setHasAllowReassoc(true);
  EXPECT_TRUE(FAdd->hasAllowReassoc());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasAllowReassoc());

  Chkpnt.save();
  FAdd->setHasNoNaNs(true);
  EXPECT_TRUE(FAdd->hasNoNaNs());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasNoNaNs());

  Chkpnt.save();
  FAdd->setHasNoInfs(true);
  EXPECT_TRUE(FAdd->hasNoInfs());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasNoInfs());

  Chkpnt.save();
  FAdd->setHasNoSignedZeros(true);
  EXPECT_TRUE(FAdd->hasNoSignedZeros());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasNoSignedZeros());

  Chkpnt.save();
  FAdd->setHasAllowReciprocal(true);
  EXPECT_TRUE(FAdd->hasAllowReciprocal());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasAllowReciprocal());

  Chkpnt.save();
  FAdd->setHasAllowContract(true);
  EXPECT_TRUE(FAdd->hasAllowContract());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasAllowContract());

  Chkpnt.save();
  FAdd->setHasApproxFunc(true);
  EXPECT_TRUE(FAdd->hasApproxFunc());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->hasApproxFunc());

  Chkpnt.save();
  FAdd->copyFastMathFlags(FastMathFlags::getFast());
  EXPECT_TRUE(FAdd->isFast());
  Chkpnt.rollback();
  EXPECT_FALSE(FAdd->isFast());

  Chkpnt.save();
  GEP->setIsInBounds(true);
  EXPECT_TRUE(GEP->isInBounds());
  Chkpnt.rollback();
  EXPECT_FALSE(GEP->isInBounds());
}

TEST(CheckpointTest, SetGlobalValueSubClassData) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  F->setIsMaterializable(false);
  Chkpnt.save();

  F->setIsMaterializable(true);
  Chkpnt.rollback();
  EXPECT_FALSE(F->isMaterializable());
}

TEST(CheckpointTest, SetNumHungOffUseOperands) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
entry:
  br label %bb1

bb1:
  %phi = phi i32 [ 0, %entry ], [ %a, %bb1 ]
  br label %bb1
}
)");
  Function *F = &*M->begin();
  BasicBlock *EntryBB = getBBWithName(F, "entry");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  Argument *Arg0 = F->getArg(0);
  auto It = BB1->begin();
  PHINode *Phi = cast<PHINode>(&*It++);
  Value *Zero = Phi->getIncomingValue(0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  Phi->setNumHungOffUseOperands(0);
  ASSERT_EQ(Phi->getNumIncomingValues(), 0u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ASSERT_EQ(Phi->getNumIncomingValues(), 2u);
  ASSERT_EQ(Phi->getIncomingBlock(0), EntryBB);
  ASSERT_EQ(Phi->getIncomingValue(0), Zero);
  ASSERT_EQ(Phi->getIncomingBlock(1), BB1);
  ASSERT_EQ(Phi->getIncomingValue(1), Arg0);
}

TEST(CheckpointTest, PHIIncomingValues) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
entry:
  br label %bb1

bb1:
  %phi = phi i32 [ 0, %entry ], [ %a, %bb1 ]
  br label %bb1
}
)");
  Function *F = &*M->begin();
  BasicBlock *EntryBB = getBBWithName(F, "entry");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  Argument *Arg0 = F->getArg(0);
  auto It = BB1->begin();
  PHINode *Phi = cast<PHINode>(&*It++);
  Value *Zero = Phi->getIncomingValue(0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Check setIncomingValue()
  Chkpnt.save();
  EXPECT_EQ(Phi->getIncomingValue(0), Zero);
  Phi->setIncomingValue(0, Arg0);
  EXPECT_EQ(Phi->getIncomingValue(0), Arg0);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(Phi->getIncomingValue(0), Zero);

  // Check setIncomingBlock()
  Chkpnt.save();
  EXPECT_EQ(Phi->getIncomingBlock(0), EntryBB);
  Phi->setIncomingBlock(0, BB1);
  EXPECT_EQ(Phi->getIncomingBlock(0), BB1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(Phi->getIncomingBlock(0), EntryBB);

  // Check removeIncomingValue()
  Chkpnt.save();
  Phi->removeIncomingValue(EntryBB);
  EXPECT_EQ(Phi->getNumIncomingValues(), 1u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(Phi->getNumIncomingValues(), 2u);
  EXPECT_EQ(Phi->getIncomingValue(0), Zero);
  EXPECT_EQ(Phi->getIncomingBlock(0), EntryBB);
  EXPECT_EQ(Phi->getIncomingValue(1), Arg0);
  EXPECT_EQ(Phi->getIncomingBlock(1), BB1);

  // Check addIncoming()
  Phi->removeIncomingValue(EntryBB);
  Chkpnt.save();
  // Note that the saved phi is: %phi = phi i32 [ %a, %bb1 ]
  EXPECT_EQ(Phi->getNumIncomingValues(), 1u);
  EXPECT_EQ(Phi->getIncomingValue(0), Arg0);
  EXPECT_EQ(Phi->getIncomingBlock(0), BB1);
  Phi->addIncoming(Zero, EntryBB);
  EXPECT_EQ(Phi->getNumIncomingValues(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(Phi->getNumIncomingValues(), 1u);
  EXPECT_EQ(Phi->getIncomingValue(0), Arg0);
  EXPECT_EQ(Phi->getIncomingBlock(0), BB1);
}

TEST(CheckpointTest, FnAttributes) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
entry:
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Test adding attribute.
  Chkpnt.save();
  AttributeList List =
      AttributeList::get(C, AttributeList::FunctionIndex,
                         {Attribute::AlwaysInline, Attribute::NonNull});
  F->setAttributes(List);
  EXPECT_EQ(F->getAttributes(), List);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_TRUE(F->getAttributes().isEmpty());

  // Test removing attributes.
  F->setAttributes(List);
  Chkpnt.save();
  AttributeMask Mask;
  Mask.addAttribute(Attribute::get(C, Attribute::AlwaysInline));
  F->removeFnAttrs(Mask);
  EXPECT_NE(F->getAttributes(), List);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(F->getAttributes(), List);
}

TEST(CheckpointTest, CallBaseAttributes) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
entry:
  call void @foo(i32 %a)
  ret void
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  BasicBlock *EntryBB = &*F->begin();
  CallBase *CB = cast<CallBase>(&*EntryBB->begin());

  // Test adding attribute.
  Chkpnt.save();
  CB->addFnAttr(Attribute::AlwaysInline);
  EXPECT_FALSE(CB->getAttributes().isEmpty());
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_TRUE(CB->getAttributes().isEmpty());

  // Test removing attributes.
  CB->addFnAttr(Attribute::AlwaysInline);
  Chkpnt.save();
  CB->removeAttributeAtIndex(AttributeList::FunctionIndex,
                             Attribute::AlwaysInline);
  EXPECT_FALSE(CB->hasFnAttr(Attribute::AlwaysInline));
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_TRUE(CB->hasFnAttr(Attribute::AlwaysInline));
}

TEST(CheckpointTest, GlobalVariableAttributes) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = external global i32
)");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Test setting attributes.
  Chkpnt.save();
  AttributeSet Attrs =
      AttributeSet::get(C, ArrayRef(Attribute::get(C, Attribute::NonNull)));
  GV->setAttributes(Attrs);
  EXPECT_EQ(GV->getAttributes(), Attrs);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(GV->getAttributes().getNumAttributes(), 0u);
}

TEST(CheckpointTest, GlobalValueProperties) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = weak constant i32 1
)");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  auto CheckOrigState = [GV]() {
    EXPECT_EQ(GV->getLinkage(), GlobalValue::WeakAnyLinkage);
    EXPECT_EQ(GV->getVisibility(), GlobalValue::DefaultVisibility);
    EXPECT_EQ(GV->getUnnamedAddr(), GlobalValue::UnnamedAddr::None);
    EXPECT_EQ(GV->getDLLStorageClass(), GlobalValue::DefaultStorageClass);
    EXPECT_FALSE(GV->isThreadLocal());
    EXPECT_FALSE(GV->isDSOLocal());
    EXPECT_FALSE(GV->hasPartition());
    EXPECT_FALSE(GV->hasSanitizerMetadata());
    EXPECT_FALSE(GV->hasPartition());
  };
  CheckOrigState();
  Chkpnt.save();

  GV->setLinkage(GlobalValue::ExternalLinkage);
  GV->setVisibility(GlobalValue::ProtectedVisibility);
  GV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  GV->setDLLStorageClass(GlobalValue::DLLExportStorageClass);
  GV->setThreadLocal(true);
  GV->setDSOLocal(true);
  GV->setPartition("Partition");
  GlobalValue::SanitizerMetadata SMD;
  SMD.NoAddress = 1;
  GV->setSanitizerMetadata(SMD);

  Chkpnt.rollback();
  CheckOrigState();
}

TEST(CheckpointTest, GlobalVariableInitializer) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = weak constant i32 42
)");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);
  EXPECT_EQ(GV->getInitializer(), FortyTwo);
  Chkpnt.save();
  auto *One = ConstantInt::get(Type::getInt32Ty(C), 1);
  GV->setInitializer(One);

  Chkpnt.rollback();
  EXPECT_EQ(GV->getInitializer(), FortyTwo);
}

TEST(CheckpointTest, CallInst) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
entry:
  call void @foo(i32 %a)
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *EntryBB = getBBWithName(F, "entry");
  auto It = EntryBB->begin();
  CallInst *Call = cast<CallInst>(&*It++);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  CallInst::TailCallKind OrigTCK = Call->getTailCallKind();

  // Check setTailCallKind()
  Chkpnt.save();
  ASSERT_NE(Call->getTailCallKind(), CallInst::TailCallKind::TCK_MustTail);
  Call->setTailCallKind(CallInst::TailCallKind::TCK_MustTail);
  ASSERT_EQ(Call->getTailCallKind(), CallInst::TailCallKind::TCK_MustTail);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ASSERT_EQ(Call->getTailCallKind(), OrigTCK);

  // Check setCanReturnTwice()
  Chkpnt.save();
  ASSERT_FALSE(Call->canReturnTwice());
  Call->setCanReturnTwice();
  ASSERT_TRUE(Call->canReturnTwice());
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ASSERT_FALSE(Call->canReturnTwice());
}

TEST(CheckpointTest, RAUW) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %with = add i32 %b, %b
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  auto It = BB0->begin();
  auto *With = &*It++;
  auto *Ret = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Arg0->replaceAllUsesWith(With);
  EXPECT_NE(Ret->getOperand(0), Arg0);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Ret->getOperand(0), Arg0);
}

TEST(CheckpointTest, RUWIf) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %with = add i32 %b, %b
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  auto It = BB0->begin();
  auto *With = &*It++;
  auto *Ret = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Arg0->replaceUsesWithIf(With, [](Use &U) { return true; });
  EXPECT_NE(Ret->getOperand(0), Arg0);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Ret->getOperand(0), Arg0);
}

TEST(CheckpointTest, RUOW) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->replaceUsesOfWith(Arg0, Arg1);
  EXPECT_NE(Instr->getOperand(0), Arg0);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getOperand(0), Arg0);
}

TEST(CheckpointTest, ConstantUsers) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, 42
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr = &*It++;
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  EXPECT_TRUE(FortyTwo->hasNUses(1u));
  Instr->setOperand(1, Arg1);
  EXPECT_TRUE(FortyTwo->hasNUses(0u));
  Instr->setOperand(1, FortyTwo);
  Instr->setOperand(0, FortyTwo);
  EXPECT_TRUE(FortyTwo->hasNUses(2u));
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_TRUE(FortyTwo->hasNUses(1u));
}

TEST(CheckpointTest, DropAllReferences) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr->dropAllReferences();
  EXPECT_NE(Instr->getOperand(0), Arg0);
  EXPECT_NE(Instr->getOperand(1), Arg1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(Instr->getOperand(0), Arg0);
  EXPECT_EQ(Instr->getOperand(1), Arg1);
}

// Checks that we can rollback a sequence of changes.
TEST(CheckpointTest, MultipleChanges01) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %instr = add i32 %a, %b
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr = &*It++;

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  auto *Mul = BinaryOperator::CreateMul(Arg0, Arg1);  // mul %a, %b
  Mul->setOperand(0, Instr);                          // mul %instr, %b
  Mul->setOperand(1, Arg0);                           // mul %instr, %a
  Mul->insertAfter(Instr);
  Mul->moveBefore(Instr);
  Instr->setOperand(1, Arg0);
  //bb0:
  //  %0 = mul i32 %instr, %a
  //  %instr = add i32 %a, %a
  //  ret i32 %a
  EXPECT_NE(BB0->size(), 2u);
  EXPECT_NE(Instr->getPrevNode(), nullptr);
  EXPECT_NE(Instr->getOperand(1), Arg1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 2u);
  EXPECT_EQ(Instr->getPrevNode(), nullptr);
  EXPECT_EQ(Instr->getOperand(1), Arg1);
}

TEST(CheckpointTest, MultipleChanges02) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %instr2 = mul i32 %instr1, %b
  %instr3 = sub i32 %instr2, 42
  ret i32 %a
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*F->begin();
  Argument *Arg0 = F->getArg(0);
  Argument *Arg1 = F->getArg(1);
  auto It = BB0->begin();
  auto *Instr1 = &*It++;
  auto *Instr2 = &*It++;
  auto *Instr3 = &*It++;
  auto *Ret = &*It++;
  auto *FortyTwo = ConstantInt::get(Type::getInt32Ty(C), 42);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  Instr1->moveAfter(Ret);
  Instr2->eraseFromParent();
  Instr3->setOperand(0, Arg0);
  Instr3->setOperand(0, Arg1);
  //bb0:
  //  %instr3 = sub i32 %b, 42
  //  ret i32 %a
  //  %instr1 = add i32 %a, %b
  EXPECT_NE(BB0->size(), 4u);
  EXPECT_NE(&*std::next(BB0->begin(), 0), Instr1);
  EXPECT_NE(&*std::next(BB0->begin(), 1), Instr2);
  EXPECT_NE(&*std::next(BB0->begin(), 2), Instr3);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();

  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_EQ(&*std::next(BB0->begin(), 0), Instr1);
  EXPECT_EQ(&*std::next(BB0->begin(), 1), Instr2);
  EXPECT_EQ(&*std::next(BB0->begin(), 2), Instr3);
  EXPECT_EQ(Instr1->getOperand(0), Arg0);
  EXPECT_EQ(Instr1->getOperand(1), Arg1);
  EXPECT_EQ(Instr2->getOperand(0), Instr1);
  EXPECT_EQ(Instr2->getOperand(1), Arg1);
  EXPECT_EQ(Instr3->getOperand(0), Instr2);
  EXPECT_EQ(Instr3->getOperand(1), FortyTwo);
}

TEST(CheckpointTest, RemoveBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  br label %bb1

bb1:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  EXPECT_EQ(F->size(), 2u);
  BB0->removeFromParent();
  EXPECT_EQ(F->size(), 1u);
  BB1->removeFromParent();
  EXPECT_EQ(F->size(), 0u);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(F->size(), 2u);
}

TEST(CheckpointTest, EraseBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  br label %bb1

bb1:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  EXPECT_EQ(F->size(), 2u);
  auto It0 = BB0->eraseFromParent();
  EXPECT_EQ(It0, BB1->getIterator());
  EXPECT_EQ(F->size(), 1u);
  auto It1 = BB1->eraseFromParent();
  EXPECT_EQ(It1, F->end());
  EXPECT_EQ(F->size(), 0u);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(F->size(), 2u);
}

TEST(CheckpointTest, DeleteBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  br label %bb1

bb1:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  BB0->removeFromParent();
  BB1->removeFromParent();
  delete BB0;
  delete BB1;

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(F->size(), 2u);
  EXPECT_EQ(BB0->getName(), "bb0");
  EXPECT_EQ(BB1->getName(), "bb1");
}

TEST(CheckpointTest, EraseInstrRange) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %instr2 = mul i32 %instr1, %b
  %instr3 = sub i32 %instr2, 42
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  auto It = BB0->begin();
  Instruction *Instr1 = &*It++;
  Instruction *Instr2 = &*It++;
  Instruction *Instr3 = &*It++;
  Instruction *Ret = &*It++;

  EXPECT_EQ(BB0->size(), 4u);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Erase first
  Chkpnt.save();
  BB0->erase(BB0->begin(), std::next(BB0->begin()));
  EXPECT_EQ(BB0->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);

  // Erase last
  Chkpnt.save();
  BB0->erase(std::prev(BB0->end()), BB0->end());
  EXPECT_EQ(BB0->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);

  // Erase middle two
  Chkpnt.save();
  BB0->erase(std::next(BB0->begin(), 1), std::next(BB0->end(), 4));
  EXPECT_EQ(BB0->size(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);

  // Erase all
  Chkpnt.save();
  BB0->erase(BB0->begin(), BB0->end());
  EXPECT_EQ(BB0->size(), 0u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);

  // Erase none 1
  Chkpnt.save();
  BB0->erase(BB0->begin(), BB0->begin());
  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_TRUE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);

  // Erase none 2
  Chkpnt.save();
  BB0->erase(std::next(BB0->begin()), std::next(BB0->begin()));
  EXPECT_EQ(BB0->size(), 4u);
  EXPECT_TRUE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 4u);
  It = BB0->begin();
  EXPECT_EQ(&*It++, Instr1);
  EXPECT_EQ(&*It++, Instr2);
  EXPECT_EQ(&*It++, Instr3);
  EXPECT_EQ(&*It++, Ret);
}

TEST(CheckpointTest, SpliceBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %instr2 = mul i32 %instr1, %b
  br label %bb1

bb1:
  %instr3 = sub i32 %instr2, 42
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  auto It = BB0->begin();
  Instruction *Instr1 = &*It++;
  Instruction *Instr2 = &*It++;
  Instruction *Br = &*It++;
  It = BB1->begin();
  Instruction *Instr3 = &*It++;
  Instruction *Ret = &*It++;
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Checks that BB0 and BB1 are exactly like the original code above.
  auto ExpectSameAsOrig = [Instr1, Instr2, Instr3, Br, Ret](BasicBlock *BB0,
                                                            BasicBlock *BB1) {
    EXPECT_EQ(BB0->size(), 3u);
    auto It = BB0->begin();
    EXPECT_EQ(&*It++, Instr1);
    EXPECT_EQ(&*It++, Instr2);
    EXPECT_EQ(&*It++, Br);
    EXPECT_EQ(BB1->size(), 2u);
    It = BB1->begin();
    EXPECT_EQ(&*It++, Instr3);
    EXPECT_EQ(&*It++, Ret);
  };

  EXPECT_EQ(BB0->size(), 3u);
  EXPECT_EQ(BB1->size(), 2u);

  // Splice 2 instructions from BB0 to beginning of BB1
  Chkpnt.save();
  BB1->splice(BB1->begin(), BB0, BB0->begin(), std::next(BB0->begin(), 2));
  EXPECT_EQ(BB0->size(), 1u);
  EXPECT_EQ(BB1->size(), 4u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);

  // Splice first instruction from BB0 to beginning of BB1
  Chkpnt.save();
  BB1->splice(BB1->begin(), BB0, BB0->begin(), std::next(BB0->begin(), 1));
  EXPECT_EQ(BB0->size(), 2u);
  EXPECT_EQ(BB1->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);

  // Splice no instrs from BB0 to BB1 (when FromBeginIt == FromEndIt)
  Chkpnt.save();
  BB1->splice(BB1->begin(), BB0, BB0->begin(), BB0->begin());
  ExpectSameAsOrig(BB0, BB1);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);

  // Splice last instruction from BB0 to beginning of BB1
  Chkpnt.save();
  BB1->splice(BB1->begin(), BB0, std::prev(BB0->end()), BB0->end());
  EXPECT_EQ(BB0->size(), 2u);
  EXPECT_EQ(BB1->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);

  // Splice last instruction from BB0 to end of BB1
  Chkpnt.save();
  BB1->splice(BB1->end(), BB0, std::prev(BB0->end()), BB0->end());
  EXPECT_EQ(BB0->size(), 2u);
  EXPECT_EQ(BB1->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);

  // Splice 1 instruction Source == Destination.
  Chkpnt.save();
  BB0->splice(BB0->begin(), BB0, BB0->begin());
  EXPECT_EQ(BB0->size(), 3u);
  EXPECT_EQ(BB1->size(), 2u);
  EXPECT_TRUE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(BB0, BB1);
}

TEST(CheckpointTest, SpliceFn) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  br label %bb1
bb1:
  ret void
}

define void @bar() {
bb2:
  br label %bb3
bb3:
  br label %bb4
bb4:
  ret void
}
)");
  Function *Foo = M->getFunction("foo");
  Function *Bar = M->getFunction("bar");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Splice all BBs from Foo to Bar.
  Chkpnt.save();
  Bar->splice(Bar->begin(), Foo);
  EXPECT_TRUE(Foo->empty());
  EXPECT_EQ(Bar->size(), 5u);
  Chkpnt.rollback();
  EXPECT_EQ(Foo->size(), 2u);
  EXPECT_EQ(Bar->size(), 3u);

  // Transfer one BB from Foo to Bar.
  Chkpnt.save();
  Bar->splice(std::next(Bar->begin()), Foo, Foo->begin());
  EXPECT_EQ(Foo->size(), 1u);
  EXPECT_EQ(Bar->size(), 4u);
  Chkpnt.rollback();
  EXPECT_EQ(Foo->size(), 2u);
  EXPECT_EQ(Bar->size(), 3u);

  // Transfer a range of BBs from Foo to Bar.
  Chkpnt.save();
  Bar->splice(Bar->end(), Foo, Foo->begin(), Foo->end());
  EXPECT_EQ(Foo->size(), 0u);
  EXPECT_EQ(Bar->size(), 5u);
  Chkpnt.rollback();
  EXPECT_EQ(Foo->size(), 2u);
  EXPECT_EQ(Bar->size(), 3u);
}

enum class SplitFnTy {
  SplitBB_BeforeFalse,
  SplitBB_BeforeTrue,
  SplitBBBefore_Iterator,
  SplitBBBefore_Instr,
};

static void splitBB(SplitFnTy SplitFnTy) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  %instr2 = mul i32 %instr1, %b
  %instr3 = sub i32 %instr2, 42
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  auto It = BB0->begin();
  Instruction *Instr1 = &*It++;
  Instruction *Instr2 = &*It++;
  Instruction *Instr3 = &*It++;
  Instruction *Ret = &*It++;

  // Checks that BB0 and BB1 are exactly like the original code above.
  auto ExpectSameAsOrig = [Instr1, Instr2, Instr3, Ret](Function *F) {
    EXPECT_EQ(F->size(), 1u);
    BasicBlock *BB0 = &*F->begin();
    EXPECT_EQ(BB0->size(), 4u);
    auto It = BB0->begin();
    EXPECT_EQ(&*It++, Instr1);
    EXPECT_EQ(&*It++, Instr2);
    EXPECT_EQ(&*It++, Instr3);
    EXPECT_EQ(&*It++, Ret);
    EXPECT_EQ(BB0->getName(), "bb0");
  };
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Split BB0 at Instr2.
  Chkpnt.save();
  switch (SplitFnTy) {
    case SplitFnTy::SplitBB_BeforeFalse:
      BB0->splitBasicBlock(Instr2->getIterator(), "NewBB", /*Before=*/false);
      EXPECT_EQ(BB0->size(), 2u);
      break;
    case SplitFnTy::SplitBB_BeforeTrue:
      BB0->splitBasicBlock(Instr2->getIterator(), "NewBB", /*Before=*/true);
      EXPECT_EQ(BB0->size(), 3u);
      break;
    case SplitFnTy::SplitBBBefore_Iterator:
      BB0->splitBasicBlockBefore(Instr2->getIterator(), "NewBB");
      EXPECT_EQ(BB0->size(), 3u);
      break;
    case SplitFnTy::SplitBBBefore_Instr:
      BB0->splitBasicBlockBefore(Instr2, "NewBB");
      EXPECT_EQ(BB0->size(), 3u);
      break;
  }
  EXPECT_EQ(F->size(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(F);
}

TEST(CheckpointTest, SplitBB) {
  splitBB(SplitFnTy::SplitBB_BeforeFalse);
  splitBB(SplitFnTy::SplitBB_BeforeTrue);
  splitBB(SplitFnTy::SplitBBBefore_Iterator);
  splitBB(SplitFnTy::SplitBBBefore_Instr);
}

TEST(CheckpointTest, RemovePredecessor) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
entry:
  br label %bb1

bb1:
  %phi = phi i32 [ 0, %entry ], [ %instr2, %bb2 ]
  %instr1 = add i32 %a, %phi
  br label %bb2

bb2:
  %instr2 = sub i32 %instr1, 42
  br label %bb1
}
)");
  Function *F = &*M->begin();
  BasicBlock *EntryBB = getBBWithName(F, "entry");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  BasicBlock *BB2 = getBBWithName(F, "bb2");
  auto It = EntryBB->begin();
  Instruction *EntryBr = &*It++;
  It = BB1->begin();
  auto *Phi = cast<PHINode>(&*It++);
  Value *PhiVal0 = Phi->getIncomingValue(0);
  Value *PhiVal1 = Phi->getIncomingValue(1);
  BasicBlock *PhiBB0 = EntryBB;
  BasicBlock *PhiBB1 = BB2;
  Instruction *Instr1 = &*It++;
  Instruction *Br = &*It++;
  It = BB2->begin();
  Instruction *Instr2 = &*It++;
  Instruction *Ret = &*It++;
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  // Sanity checks
  EXPECT_EQ(EntryBB->size(), 1u);
  EXPECT_EQ(BB1->size(), 3u);
  EXPECT_EQ(BB2->size(), 2u);

  Chkpnt.save();

  // Checks that F looks like the original code above.
  auto ExpectSameAsOrig = [EntryBB, BB1, BB2, EntryBr, Phi, PhiVal0, PhiVal1,
                           PhiBB0, PhiBB1, Instr1, Br, Instr2,
                           Ret](Function *F) {
    EXPECT_EQ(EntryBB, getBBWithName(F, "entry"));
    EXPECT_EQ(BB1, getBBWithName(F, "bb1"));
    EXPECT_EQ(BB2, getBBWithName(F, "bb2"));
    auto It = EntryBB->begin();
    EXPECT_EQ(EntryBB->size(), 1u);
    EXPECT_EQ(&*It++, EntryBr);

    It = BB1->begin();
    EXPECT_EQ(BB1->size(), 3u);
    EXPECT_EQ(&*It++, Phi);
    EXPECT_EQ(Phi->getNumIncomingValues(), 2u);
    EXPECT_EQ(Phi->getIncomingValue(0), PhiVal0);
    EXPECT_EQ(Phi->getIncomingValue(1), PhiVal1);
    EXPECT_EQ(Phi->getIncomingBlock(0), PhiBB0);
    EXPECT_EQ(Phi->getIncomingBlock(1), PhiBB1);
    EXPECT_EQ(&*It++, Instr1);
    EXPECT_EQ(&*It++, Br);

    It = BB2->begin();
    EXPECT_EQ(BB2->size(), 2u);
    EXPECT_EQ(&*It++, Instr2);
    EXPECT_EQ(&*It++, Ret);
  };

  BB1->removePredecessor(BB2);
  EXPECT_EQ(BB1->size(), 2u);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  ExpectSameAsOrig(F);
}

TEST(CheckpointTest, MoveAfterBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  br label %bb1

bb1:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  BasicBlock *BB1 = &*std::next(F->begin(), 1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  BB0->moveAfter(BB1);
  EXPECT_NE(BB0->getNextNode(), BB1);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->getNextNode(), BB1);
}

TEST(CheckpointTest, MoveBeforeBB) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  br label %bb1

bb1:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  BasicBlock *BB1 = &*std::next(F->begin(), 1);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  BB1->moveBefore(BB0);
  EXPECT_NE(BB0->getNextNode(), BB1);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->getNextNode(), BB1);
}

TEST(CheckpointTest, MaxNumOfTrackedChanges) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save(/*MaxNumOfTrackedChanges=*/2);
  BB0->setName("change1");
#ifndef NDEBUG
  EXPECT_DEATH(BB0->setName("change2"), "Tracking too many changes!");
#endif
  Chkpnt.accept();
}

TEST(CheckpointTest, CreateValue) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  Argument *ArgA = F->getArg(0);
  Argument *ArgB = F->getArg(1);
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  Instruction *Ret = BB0->getTerminator();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  auto *NewI = BinaryOperator::CreateAdd(ArgA, ArgB);
  NewI->insertBefore(Ret);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
#ifndef NDEBUG
  EXPECT_DEATH(NewI->deleteValue(), ".*");
#endif // NDEBUG
}

TEST(CheckpointTest, DeleteValue) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a, i32 %b) {
bb0:
  %instr1 = add i32 %a, %b
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  auto It = BB0->begin();
  Instruction *Instr1 = &*It++;
  Instruction *Ret = &*It++;
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Instr1->removeFromParent();

  Chkpnt.save();
  Instr1->deleteValue();
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  Instr1->insertBefore(Ret);

  Chkpnt.save();
  Instr1->eraseFromParent();
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BB0->size(), 2u);
}

TEST(CheckpointTest, EraseFunction) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  F->eraseFromParent();
  Chkpnt.rollback();
}

TEST(CheckpointTest, RemoveFunction) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  F->removeFromParent();
  Chkpnt.rollback();
}

TEST(CheckpointTest, ValueHandle) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i32 %a) {
bb0:
  ret void
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = &*std::next(F->begin(), 0);
  Instruction *Ret = BB0->getTerminator();
  Argument *ArgA = F->getArg(0);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  auto *NewI = BinaryOperator::CreateAdd(ArgA, ArgA);
  NewI->insertBefore(Ret);
  AssertingVH<Instruction> AssertVH(NewI);

  EXPECT_FALSE(Chkpnt.empty());
#ifndef NDEBUG
  // Rollback will delete NewI, but AssertVH is still watching it.
  EXPECT_DEATH(Chkpnt.rollback(),
               "An asserting value handle still pointed to this value!");
#endif
  // EXPECT_DEATH() creates a new process, so this process won't rollback().
  Chkpnt.accept();
}

TEST(CheckpointTest, CreateConstant) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  ConstantInt::get(Type::getInt32Ty(C), 1, true);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, CreateConstantConstantUniqueMap) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @F() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  SmallVector<Constant*, 1> ConstVec;
  Type *ElmTy = Type::getInt1Ty(C);
  auto *C0 = ConstantInt::get(ElmTy, 1);
  ConstVec.push_back(C0);
  ArrayType *Ty = ArrayType::get(ElmTy, 1);
  Chkpnt.save();
  ConstantArray::get(Ty, ConstVec);
  // We can't check pImpl->ArrayConstants so just check Chkpnt.size()
  // to make sure the insertion to ConstantUniqueMap is tracked.
#ifndef NDEBUG
  EXPECT_EQ(Chkpnt.size(), 3u);
#endif
  Chkpnt.rollback();
}

TEST(CheckpointTest, ConstantRemoveFromConstantUniqueMap) {
  // Make sure
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = internal global [1 x ptr] [ptr @F]
define void @F() {
bb0:
  ret void
}
)");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  auto *GV = cast<GlobalVariable>(M->getNamedValue("GV"));
  Constant *Init = GV->getInitializer();
  Chkpnt.save();
  GV->setInitializer(nullptr);
  Init->destroyConstant();
  Chkpnt.rollback();
}

TEST(CheckpointTest, CreateFunction) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo() {
  ret void
}
)");
  Function *F = &*M->begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  ASSERT_EQ(M->size(), 1u);
  Function::Create(FunctionType::get(Type::getVoidTy(C), false),
                   GlobalValue::ExternalLinkage, "NewF", *M);
  EXPECT_EQ(M->size(), 2u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(M->size(), 1u);
  EXPECT_EQ(&*M->begin(), F);
}

//===----------------------------------------------------------------------===//
// Things that are not supported yet
//===----------------------------------------------------------------------===//

TEST(CheckpointTest, EdgeProbability) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i1 %cond) {
bb0:
  br i1 %cond, label %bb1, label %bb2

bb1:
  ret void

bb2:
  ret void
}
)");
  Function *F = M->getFunction("foo");
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  BasicBlock *BB2 = getBBWithName(F, "bb2");
  DominatorTree DT(*F);
  LoopInfo LI(DT);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  BranchProbabilityInfo BPI(*F, LI);
  auto OrigEdgeProbBB0ToBB1 = BPI.getEdgeProbability(BB0, BB1);
  auto OrigEdgeProbBB0ToBB2 = BPI.getEdgeProbability(BB0, BB2);
  SmallVector<BranchProbability, 2> Probs;
  Probs.push_back(BranchProbability(1, 100));
  Probs.push_back(BranchProbability(99, 100));
  BPI.setEdgeProbability(BB0, Probs);
  EXPECT_NE(BPI.getEdgeProbability(BB0, BB1), OrigEdgeProbBB0ToBB1);
  Chkpnt.rollback();

  // TODO: Change these to EXPECT_EQ we add support for branch probabity info.
  EXPECT_NE(BPI.getEdgeProbability(BB0, BB1), OrigEdgeProbBB0ToBB1);
  EXPECT_NE(BPI.getEdgeProbability(BB0, BB2), OrigEdgeProbBB0ToBB2);
}

TEST(CheckpointTest, BlockFrequency) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i1 %cond) {
bb0:
  br i1 %cond, label %bb1, label %bb2

bb1:
  ret void

bb2:
  ret void
}
)");
  Function *F = M->getFunction("foo");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  BasicBlock *BB2 = getBBWithName(F, "bb2");
  DominatorTree DT(*F);
  LoopInfo LI(DT);
  BranchProbabilityInfo BPI(*F, LI);
  BlockFrequencyInfo BFI(*F, BPI, LI);
  BlockFrequency OrigBB1Freq = BFI.getBlockFreq(BB1);
  BlockFrequency OrigBB2Freq = BFI.getBlockFreq(BB2);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  BFI.setBlockFreq(BB1, 42);
  BFI.setBlockFreq(BB2, 43);
  EXPECT_FALSE(BFI.getBlockFreq(BB1) == OrigBB1Freq);
  EXPECT_FALSE(BFI.getBlockFreq(BB2) == OrigBB2Freq);
  Chkpnt.rollback();

  // TODO: Change these to EXPECT_TRUE we add support for block frequencies.
  EXPECT_FALSE(BFI.getBlockFreq(BB1) == OrigBB1Freq);
  EXPECT_FALSE(BFI.getBlockFreq(BB2) == OrigBB2Freq);
}


//===----------------------------------------------------------------------===//
// This section contains more complicated tests that create tens or hundreds of
// change bojects.
// Most of these tests are copied from other tests.
//===----------------------------------------------------------------------===//


// For now we don't maintain the order in the use list.
// So code that looks like this:
//   bb2:  ; preds = %bb2, %bb1, %entry
// May look like this after rollback:
//   bb2:  ; preds = %bb1, %bb2, %entry
TEST(CheckpointTest, BBPredsOrder) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define void @foo(i1 %cond) {
entry:
  br label %bb2

bb1:
  br label %bb2

bb2:
  br label %bb2
}
)");
  Function *F = M->getFunction("foo");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  BB1->eraseFromParent();
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, FunctionInlining) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
foo_bb0:
  %add = add i32 %a, %b
  %sub = sub i32 %add, 42
  ret i32 %sub
}

define i32 @bar(i32 %a, i32 %b) {
bar_bb0:
  %ret = call i32 @foo(i32 %a, i32 %b)
  ret i32 %ret
}
)");
  Function *BarF = M->getFunction("bar");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  BasicBlock *BarBB0 = &*BarF->begin();
  auto It = BarBB0->begin();
  CallBase *CB = cast<CallBase>(&*It++);


  Chkpnt.save();
  ASSERT_EQ(BarBB0->size(), 2u);
  InlineFunctionInfo IFI;
  InlineFunction(*CB, IFI);
  ASSERT_EQ(BarBB0->size(), 3u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BarBB0->size(), 2u);
}

TEST(CheckpointTest, FunctionInlinineLarge) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b, i1 %cond) {
foo_bb0:
  %add = add i32 %a, %b
  br i1 %cond, label %foo_bb1, label %foo_bb2

foo_bb1:
  %sub = sub i32 %add, 42
  ret i32 %sub

foo_bb2:
  %add2 = add i32 %add, 42
  ret i32 %add2
}

define i32 @bar(i32 %a, i32 %b, i1 %cond) {
bar_bb0:
  %ret = call i32 @foo(i32 %a, i32 %b, i1 %cond)
  ret i32 %ret
}
)");
  Function *BarF = M->getFunction("bar");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  BasicBlock *BarBB0 = getBBWithName(BarF, "bar_bb0");
  auto It = BarBB0->begin();
  CallBase *CB = cast<CallBase>(&*It++);
  Instruction *Ret = &*It++;

  Chkpnt.save();
  ASSERT_EQ(BarBB0->size(), 2u);
  InlineFunctionInfo IFI;
  InlineFunction(*CB, IFI);
  // Inlining produces this:
  // define i32 @bar(i32 %a, i32 %b, i1 %cond) {
  // bar_bb0:
  //   %add.i = add i32 %a, %b
  //   br i1 %cond, label %foo_bb1.i, label %foo_bb2.i

  // foo_bb1.i:                                        ; preds = %bar_bb0
  //   %sub.i = sub i32 %add.i, 42
  //   br label %foo.exit

  // foo_bb2.i:                                        ; preds = %bar_bb0
  //   %add2.i = add i32 %add.i, 42
  //   br label %foo.exit

  // foo.exit:                                         ; preds = %foo_bb2.i,
  // %foo_bb1.i
  //   %ret1 = phi i32 [ %sub.i, %foo_bb1.i ], [ %add2.i, %foo_bb2.i ]
  //   ret i32 %ret1
  // }
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BarF->size(), 1u);
  EXPECT_EQ(BarF->begin()->getName(), "bar_bb0");
  EXPECT_EQ(&*BarF->begin(), BarBB0);
  EXPECT_EQ(BarBB0->size(), 2u);
  It = BarBB0->begin();
  EXPECT_EQ(&*It++, CB);
  EXPECT_EQ(&*It++, Ret);
}

TEST(CheckpointTest, FunctionInliningWithConstantPropagation) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b, i1 %cond) {
foo_bb0:
  %add = add i32 %a, %b
  br i1 %cond, label %foo_bb1, label %foo_bb2

foo_bb1:
  %sub = sub i32 %add, 42
  ret i32 %sub

foo_bb2:
  %add2 = add i32 %add, 42
  ret i32 %add2
}

define i32 @bar(i32 %a, i32 %b) {
bar_bb0:
  %ret = call i32 @foo(i32 %a, i32 %b, i1 0)
  ret i32 %ret
}
)");
  Function *BarF = M->getFunction("bar");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  BasicBlock *BarBB0 = getBBWithName(BarF, "bar_bb0");
  auto It = BarBB0->begin();
  CallBase *CB = cast<CallBase>(&*It++);
  Instruction *Ret = &*It++;

  Chkpnt.save();
  ASSERT_EQ(BarBB0->size(), 2u);
  InlineFunctionInfo IFI;
  InlineFunction(*CB, IFI);
  // Inlining produces this:
  // define i32 @bar(i32 %a, i32 %b) {
  // bar_bb0:
  //   %add.i = add i32 %a, %b
  //   %add2.i = add i32 %add.i, 42
  //   ret i32 %add2.i
  // }
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BarF->size(), 1u);
  EXPECT_EQ(BarF->begin()->getName(), "bar_bb0");
  EXPECT_EQ(&*BarF->begin(), BarBB0);
  EXPECT_EQ(BarBB0->size(), 2u);
  It = BarBB0->begin();
  EXPECT_EQ(&*It++, CB);
  EXPECT_EQ(&*It++, Ret);
}

TEST(CheckpointTest, CodeExtract) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b) {
bb0:
  %add = add i32 %a, %b
  ret i32 %add
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);

  Chkpnt.save();
  SmallVector<BasicBlock *, 1> BBs;
  BBs.push_back(BB0);
  CodeExtractor CE(BBs);
  CodeExtractorAnalysisCache CEAC(*F);
  Function *NewF = CE.extractCodeRegion(CEAC);
  (void)NewF;
  EXPECT_GT(M->size(), 1u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(M->size(), 1u);
}

TEST(CheckpointTest, SimplifyCFG) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
define i32 @foo(i32 %a, i32 %b, i1 %cond) {
bb0:
  %add = add i32 %a, %b
  br i1 %cond, label %bb1, label %bb2

bb1:
  %sub = sub i32 %add, 42
  br label %bb3

bb2:
  %add2 = add i32 %add, 42
  br label %bb4

bb3:
  ret i32 %sub

bb4:
  ret i32 %add2
}
)");
  Function *F = &*M->begin();
  BasicBlock *BB3 = getBBWithName(F, "bb3");
  BasicBlock *BB4 = getBBWithName(F, "bb4");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  TargetTransformInfo TTI(M->getDataLayout());
  Chkpnt.save();
  simplifyCFG(BB3, TTI);
  simplifyCFG(BB4, TTI);
  EXPECT_LT(F->size(), 5u);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(F->size(), 5u);
}

TEST(CheckpointTest, LoopRotate_MultiDeoptExit) {
  LLVMContext C;

  std::unique_ptr<Module> M = parseIR(
    C,
    R"(
declare i32 @llvm.experimental.deoptimize.i32(...)

define i32 @test(i32 * nonnull %a, i64 %x) {
entry:
  br label %for.cond1

for.cond1:
  %idx = phi i64 [ 0, %entry ], [ %idx.next, %for.tail ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %for.tail ]
  %a.idx = getelementptr inbounds i32, i32 *%a, i64 %idx
  %val.a.idx = load i32, i32* %a.idx, align 4
  %zero.check = icmp eq i32 %val.a.idx, 0
  br i1 %zero.check, label %deopt.exit, label %for.cond2

for.cond2:
  %for.check = icmp ult i64 %idx, %x
  br i1 %for.check, label %for.body, label %return

for.body:
  br label %for.tail

for.tail:
  %sum.next = add i32 %sum, %val.a.idx
  %idx.next = add nuw nsw i64 %idx, 1
  br label %for.cond1

return:
  ret i32 %sum

deopt.exit:
  %deopt.val = call i32(...) @llvm.experimental.deoptimize.i32() [ "deopt"(i32 %val.a.idx) ]
  ret i32 %deopt.val
})"
    );
  auto *F = M->getFunction("test");
  DominatorTree DT(*F);
  LoopInfo LI(DT);
  AssumptionCache AC(*F);
  TargetTransformInfo TTI(M->getDataLayout());
  TargetLibraryInfoImpl TLII;
  TargetLibraryInfo TLI(TLII);
  ScalarEvolution SE(*F, TLI, AC, DT, LI);
  SimplifyQuery SQ(M->getDataLayout());

  Loop *L = *LI.begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  bool Ret =
      LoopRotation(L, &LI, &TTI, &AC, &DT, &SE, nullptr, SQ, true, -1, false);
  // Check that it succeeds, otherwise the checkpoint test is not very useful.
  EXPECT_TRUE(Ret);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, LoopRotate_MultiDeoptExit_Nondup) {
  LLVMContext C;

  std::unique_ptr<Module> M = parseIR(
    C,
    R"(
; Rotation should be done once, attempted twice.
; Second time fails due to non-duplicatable header.

declare i32 @llvm.experimental.deoptimize.i32(...)

declare void @nondup()

define i32 @test_nondup(i32 * nonnull %a, i64 %x) {
entry:
  br label %for.cond1

for.cond1:
  %idx = phi i64 [ 0, %entry ], [ %idx.next, %for.tail ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %for.tail ]
  %a.idx = getelementptr inbounds i32, i32 *%a, i64 %idx
  %val.a.idx = load i32, i32* %a.idx, align 4
  %zero.check = icmp eq i32 %val.a.idx, 0
  br i1 %zero.check, label %deopt.exit, label %for.cond2

for.cond2:
  call void @nondup() noduplicate
  %for.check = icmp ult i64 %idx, %x
  br i1 %for.check, label %for.body, label %return

for.body:
  br label %for.tail

for.tail:
  %sum.next = add i32 %sum, %val.a.idx
  %idx.next = add nuw nsw i64 %idx, 1
  br label %for.cond1

return:
  ret i32 %sum

deopt.exit:
  %deopt.val = call i32(...) @llvm.experimental.deoptimize.i32() [ "deopt"(i32 %val.a.idx) ]
  ret i32 %deopt.val
})"
    );
  auto *F = M->getFunction("test_nondup");
  DominatorTree DT(*F);
  LoopInfo LI(DT);
  AssumptionCache AC(*F);
  TargetTransformInfo TTI(M->getDataLayout());
  TargetLibraryInfoImpl TLII;
  TargetLibraryInfo TLI(TLII);
  ScalarEvolution SE(*F, TLI, AC, DT, LI);
  SimplifyQuery SQ(M->getDataLayout());

  Loop *L = *LI.begin();

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  bool Ret =
      LoopRotation(L, &LI, &TTI, &AC, &DT, &SE, nullptr, SQ, true, -1, false);
  // Check that it succeeds, otherwise the checkpoint test is not very useful.
  EXPECT_TRUE(Ret);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, Local_ReplaceDbgDeclare) {
  LLVMContext C;

  // Original C source to get debug info for a local variable:
  // void f() { int x; }
  std::unique_ptr<Module> M = parseIR(C,
                                      R"(
      define void @f() !dbg !8 {
      entry:
        %x = alloca i32, align 4
        call void @llvm.dbg.declare(metadata i32* %x, metadata !11, metadata !DIExpression()), !dbg !13
        call void @llvm.dbg.declare(metadata i32* %x, metadata !11, metadata !DIExpression()), !dbg !13
        ret void, !dbg !14
      }
      declare void @llvm.dbg.declare(metadata, metadata, metadata)
      !llvm.dbg.cu = !{!0}
      !llvm.module.flags = !{!3, !4}
      !0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 6.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
      !1 = !DIFile(filename: "t2.c", directory: "foo")
      !2 = !{}
      !3 = !{i32 2, !"Dwarf Version", i32 4}
      !4 = !{i32 2, !"Debug Info Version", i32 3}
      !8 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, type: !9, isLocal: false, isDefinition: true, scopeLine: 1, isOptimized: false, unit: !0, retainedNodes: !2)
      !9 = !DISubroutineType(types: !10)
      !10 = !{null}
      !11 = !DILocalVariable(name: "x", scope: !8, file: !1, line: 2, type: !12)
      !12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
      !13 = !DILocation(line: 2, column: 7, scope: !8)
      !14 = !DILocation(line: 3, column: 1, scope: !8)
      )");
  auto *GV = M->getNamedValue("f");
  auto *F = dyn_cast<Function>(GV);
  Instruction *Inst = &F->front().front();
  auto *AI = dyn_cast<AllocaInst>(Inst);
  Inst = Inst->getNextNode()->getNextNode();
  Value *NewBase = Constant::getNullValue(Type::getInt32PtrTy(C));
  DIBuilder DIB(*M);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  replaceDbgDeclare(AI, NewBase, DIB, DIExpression::ApplyOffset, 0);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, Local_SimplifyCFGWithNullAC) {
  LLVMContext Ctx;

  std::unique_ptr<Module> M = parseIR(Ctx, R"(
    declare void @true_path()
    declare void @false_path()
    declare void @llvm.assume(i1 %cond);

    define i32 @foo(i1, i32) {
    entry:
      %cmp = icmp sgt i32 %1, 0
      br i1 %cmp, label %if.bb1, label %then.bb1
    if.bb1:
      call void @true_path()
      br label %test.bb
    then.bb1:
      call void @false_path()
      br label %test.bb
    test.bb:
      %phi = phi i1 [1, %if.bb1], [%0, %then.bb1]
      call void @llvm.assume(i1 %0)
      br i1 %phi, label %if.bb2, label %then.bb2
    if.bb2:
      ret i32 %1
    then.bb2:
      ret i32 0
    }
  )");

  Function &F = *cast<Function>(M->getNamedValue("foo"));
  TargetTransformInfo TTI(M->getDataLayout());

  SimplifyCFGOptions Options{};
  Options.setAssumptionCache(nullptr);

  // Obtain BasicBlock of interest to this test, %test.bb.
  BasicBlock *TestBB = nullptr;
  for (BasicBlock &BB : F) {
    if (BB.getName().equals("test.bb")) {
      TestBB = &BB;
      break;
    }
  }
  ASSERT_TRUE(TestBB);

  DominatorTree DT(F);
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Eager);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  // %test.bb is expected to be simplified by FoldCondBranchOnPHI.
  EXPECT_TRUE(simplifyCFG(TestBB, TTI,
                          RequireAndPreserveDomTree ? &DTU : nullptr, Options));
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, Local_ChangeToUnreachable) {
  LLVMContext Ctx;

  std::unique_ptr<Module> M = parseIR(Ctx,
                                      R"(
    define internal void @foo() !dbg !6 {
    entry:
      ret void, !dbg !8
    }

    !llvm.dbg.cu = !{!0}
    !llvm.debugify = !{!3, !4}
    !llvm.module.flags = !{!5}

    !0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "debugify", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
    !1 = !DIFile(filename: "test.ll", directory: "/")
    !2 = !{}
    !3 = !{i32 1}
    !4 = !{i32 0}
    !5 = !{i32 2, !"Debug Info Version", i32 3}
    !6 = distinct !DISubprogram(name: "foo", linkageName: "foo", scope: null, file: !1, line: 1, type: !7, isLocal: true, isDefinition: true, scopeLine: 1, isOptimized: true, unit: !0, retainedNodes: !2)
    !7 = !DISubroutineType(types: !2)
    !8 = !DILocation(line: 1, column: 1, scope: !6)
  )");
  Function &F = *cast<Function>(M->getNamedValue("foo"));
  BasicBlock &BB = F.front();
  Instruction &A = BB.front();
  DebugLoc DLA = A.getDebugLoc();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  changeToUnreachable(&A);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, LoopUtils_DeleteDeadLoopNest) {
  LLVMContext C;
  std::unique_ptr<Module> M =
      parseIR(C, "define void @foo() {\n"
                 "entry:\n"
                 "  br label %for.i\n"
                 "for.i:\n"
                 "  %i = phi i64 [ 0, %entry ], [ %inc.i, %for.i.latch ]\n"
                 "  br label %for.j\n"
                 "for.j:\n"
                 "  %j = phi i64 [ 0, %for.i ], [ %inc.j, %for.j ]\n"
                 "  %inc.j = add nsw i64 %j, 1\n"
                 "  %cmp.j = icmp slt i64 %inc.j, 100\n"
                 "  br i1 %cmp.j, label %for.j, label %for.k.preheader\n"
                 "for.k.preheader:\n"
                 "  br label %for.k\n"
                 "for.k:\n"
                 "  %k = phi i64 [ %inc.k, %for.k ], [ 0, %for.k.preheader ]\n"
                 "  %inc.k = add nsw i64 %k, 1\n"
                 "  %cmp.k = icmp slt i64 %inc.k, 100\n"
                 "  br i1 %cmp.k, label %for.k, label %for.i.latch\n"
                 "for.i.latch:\n"
                 "  %inc.i = add nsw i64 %i, 1\n"
                 "  %cmp.i = icmp slt i64 %inc.i, 100\n"
                 "  br i1 %cmp.i, label %for.i, label %for.end\n"
                 "for.end:\n"
                 "  ret void\n"
                 "}\n");
  Function *F = M->getFunction("foo");
  DominatorTree DT(*F);
  TargetLibraryInfoImpl TLII;
  TargetLibraryInfo TLI(TLII);
  AssumptionCache AC(*F);
  LoopInfo LI(DT);
  ScalarEvolution SE(*F, TLI, AC, DT, LI);
  Loop *L = *LI.begin();
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  deleteDeadLoop(L, &DT, &SE, &LI);
  LI.verify(DT);
  SE.verify();
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, BasicBlockUtils_EliminateUnreachableBlocks) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"IR(
define i32 @has_unreachable(i1 %cond) {
entry:
  br i1 %cond, label %bb0, label %bb1
bb0:
  br label %bb1
bb1:
  %phi = phi i32 [ 0, %entry ], [ 1, %bb0 ]
  ret i32 %phi
bb2:
  ret i32 42
}
)IR");
  Function *F = M->getFunction("has_unreachable");
  DominatorTree DT(*F);
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Eager);

  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  EliminateUnreachableBlocks(*F, &DTU);
  EXPECT_TRUE(DT.verify());
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, BasicBlockUtils_SplitEdge_ex1) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"IR(
define void @foo(i1 %cond0) {
entry:
  br i1 %cond0, label %bb0, label %bb1
bb0:
 %0 = mul i32 1, 2
  br label %bb1
bb1:
  br label %bb2
bb2:
  ret void
}
)IR");
  Function *F = M->getFunction("foo");
  DominatorTree DT(*F);
  BasicBlock *SrcBlock;
  BasicBlock *DestBlock;
  BasicBlock *NewBB;

  SrcBlock = getBBWithName(F, "entry");
  DestBlock = getBBWithName(F, "bb0");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();
  NewBB = SplitEdge(SrcBlock, DestBlock, &DT, nullptr, nullptr);
  (void) NewBB;

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, BasicBlockUtils_SplitIndirectBrCriticalEdges) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"IR(
define void @crit_edge(i8* %tgt, i1 %cond0, i1 %cond1) {
entry:
  indirectbr i8* %tgt, [label %bb0, label %bb1, label %bb2]
bb0:
  br i1 %cond0, label %bb1, label %bb2
bb1:
  %p = phi i32 [0, %bb0], [0, %entry]
  br i1 %cond1, label %bb3, label %bb4
bb2:
  ret void
bb3:
  ret void
bb4:
  ret void
}
)IR");
  Function *F = M->getFunction("crit_edge");
  DominatorTree DT(*F);
  LoopInfo LI(DT);
  BranchProbabilityInfo BPI(*F, LI);
  BlockFrequencyInfo BFI(*F, BPI, LI);
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  BasicBlock *BB0 = getBBWithName(F, "bb0");
  BasicBlock *BB1 = getBBWithName(F, "bb1");
  BasicBlock *BB2 = getBBWithName(F, "bb2");
  auto EdgeProbBB0ToBB1 = BPI.getEdgeProbability(BB0, BB1);
  auto EdgeProbBB0ToBB2 = BPI.getEdgeProbability(BB0, BB2);
  Chkpnt.save();
  ASSERT_TRUE(SplitIndirectBrCriticalEdges(*F, /*IgnoreBlocksWithoutPHI=*/false,
                                           &BPI, &BFI));

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
  EXPECT_EQ(BPI.getEdgeProbability(BB0, BB1), EdgeProbBB0ToBB1);
  EXPECT_EQ(BPI.getEdgeProbability(BB0, BB2), EdgeProbBB0ToBB2);
}

struct ForwardingPass : public PassInfoMixin<ForwardingPass> {
  template <typename T> ForwardingPass(T &&Arg) : Func(std::forward<T>(Arg)) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    return Func(F, FAM);
  }

  std::function<PreservedAnalyses(Function &, FunctionAnalysisManager &)> Func;
};

struct CheckpointTest_MemTransferLowerTest : public testing::Test {
  PassBuilder PB;
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  ModulePassManager MPM;
  LLVMContext Context;
  std::unique_ptr<Module> M;

  CheckpointTest_MemTransferLowerTest() {
    // Register all the basic analyses with the managers.
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
  }

  BasicBlock *getBasicBlockByName(Function &F, StringRef Name) const {
    for (BasicBlock &BB : F) {
      if (BB.getName() == Name)
        return &BB;
    }
    return nullptr;
  }

  Instruction *getInstructionByOpcode(BasicBlock &BB, unsigned Opcode,
                                      unsigned Number) const {
    unsigned CurrNumber = 0;
    for (Instruction &I : BB)
      if (I.getOpcode() == Opcode) {
        ++CurrNumber;
        if (CurrNumber == Number)
          return &I;
      }
    return nullptr;
  }

  void ParseAssembly(const char *IR) {
    SMDiagnostic Error;
    M = parseAssemblyString(IR, Error, Context);
    std::string errMsg;
    raw_string_ostream os(errMsg);
    Error.print("", os);

    // A failure here means that the test itself is buggy.
    if (!M)
      report_fatal_error(os.str().c_str());
  }
};

// By semantics source and destination of llvm.memcpy.* intrinsic
// are either equal or don't overlap. Once the intrinsic is lowered
// to a loop it can be hard or impossible to reason about these facts.
// For that reason expandMemCpyAsLoop is expected to  explicitly mark
// loads from source and stores to destination as not aliasing.
TEST_F(CheckpointTest_MemTransferLowerTest, MemCpyKnownLength) {
  ParseAssembly("declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8 *, i64, i1)\n"
                "define void @foo(i8* %dst, i8* %src, i64 %n) optsize {\n"
                "entry:\n"
                "  %is_not_equal = icmp ne i8* %dst, %src\n"
                "  br i1 %is_not_equal, label %memcpy, label %exit\n"
                "memcpy:\n"
                "  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %dst, i8* %src, "
                "i64 1024, i1 false)\n"
                "  br label %exit\n"
                "exit:\n"
                "  ret void\n"
                "}\n");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  FunctionPassManager FPM;
  FPM.addPass(ForwardingPass(
      [=](Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses {
        TargetTransformInfo TTI(M->getDataLayout());
        auto *MemCpyBB = getBasicBlockByName(F, "memcpy");
        Instruction *Inst = &MemCpyBB->front();
        MemCpyInst *MemCpyI = cast<MemCpyInst>(Inst);
        auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
        expandMemCpyAsLoop(MemCpyI, TTI, &SE);
        auto *CopyLoopBB = getBasicBlockByName(F, "load-store-loop");
        Instruction *LoadInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Load, 1);
        EXPECT_NE(nullptr, LoadInst->getMetadata(LLVMContext::MD_alias_scope));
        Instruction *StoreInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Store, 1);
        EXPECT_NE(nullptr, StoreInst->getMetadata(LLVMContext::MD_noalias));
        return PreservedAnalyses::none();
      }));
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  MPM.run(*M, MAM);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST_F(CheckpointTest_MemTransferLowerTest, VecMemCpyKnownLength) {
  ParseAssembly("declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8 *, i64, i1)\n"
                "define void @foo(i8* %dst, i8* %src, i64 %n) optsize {\n"
                "entry:\n"
                "  %is_not_equal = icmp ne i8* %dst, %src\n"
                "  br i1 %is_not_equal, label %memcpy, label %exit\n"
                "memcpy:\n"
                "  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %dst, i8* %src, "
                "i64 1024, i1 false)\n"
                "  br label %exit\n"
                "exit:\n"
                "  ret void\n"
                "}\n");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  FunctionPassManager FPM;
  FPM.addPass(ForwardingPass(
      [=](Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses {
        TargetTransformInfo TTI(M->getDataLayout());
        auto *MemCpyBB = getBasicBlockByName(F, "memcpy");
        Instruction *Inst = &MemCpyBB->front();
        MemCpyInst *MemCpyI = cast<MemCpyInst>(Inst);
        auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
        expandMemCpyAsLoop(MemCpyI, TTI, &SE);
        return PreservedAnalyses::none();
      }));
  FPM.addPass(LoopVectorizePass(LoopVectorizeOptions()));
  FPM.addPass(ForwardingPass(
      [=](Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses {
        auto *TargetBB = getBasicBlockByName(F, "vector.body");
        EXPECT_NE(nullptr, TargetBB);
        return PreservedAnalyses::all();
      }));
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  MPM.run(*M, MAM);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST_F(CheckpointTest_MemTransferLowerTest, AtomicMemCpyKnownLength) {
  ParseAssembly("declare void "
                "@llvm.memcpy.element.unordered.atomic.p0i32.p0i32.i64(i32*, "
                "i32 *, i64, i32)\n"
                "define void @foo(i32* %dst, i32* %src, i64 %n) optsize {\n"
                "entry:\n"
                "  %is_not_equal = icmp ne i32* %dst, %src\n"
                "  br i1 %is_not_equal, label %memcpy, label %exit\n"
                "memcpy:\n"
                "  call void "
                "@llvm.memcpy.element.unordered.atomic.p0i32.p0i32.i64(i32* "
                "%dst, i32* %src, "
                "i64 1024, i32 4)\n"
                "  br label %exit\n"
                "exit:\n"
                "  ret void\n"
                "}\n");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  FunctionPassManager FPM;
  FPM.addPass(ForwardingPass(
      [=](Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses {
        TargetTransformInfo TTI(M->getDataLayout());
        auto *MemCpyBB = getBasicBlockByName(F, "memcpy");
        Instruction *Inst = &MemCpyBB->front();
        assert(isa<AtomicMemCpyInst>(Inst) &&
               "Expecting llvm.memcpy.p0i8.i64 instructon");
        AtomicMemCpyInst *MemCpyI = cast<AtomicMemCpyInst>(Inst);
        auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
        expandAtomicMemCpyAsLoop(MemCpyI, TTI, &SE);
        auto *CopyLoopBB = getBasicBlockByName(F, "load-store-loop");
        Instruction *LoadInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Load, 1);
        EXPECT_TRUE(LoadInst->isAtomic());
        EXPECT_NE(LoadInst->getMetadata(LLVMContext::MD_alias_scope), nullptr);
        Instruction *StoreInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Store, 1);
        EXPECT_TRUE(StoreInst->isAtomic());
        EXPECT_NE(StoreInst->getMetadata(LLVMContext::MD_noalias), nullptr);
        return PreservedAnalyses::none();
      }));
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  MPM.run(*M, MAM);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST_F(CheckpointTest_MemTransferLowerTest, AtomicMemCpyUnKnownLength) {
  ParseAssembly("declare void "
                "@llvm.memcpy.element.unordered.atomic.p0i32.p0i32.i64(i32*, "
                "i32 *, i64, i32)\n"
                "define void @foo(i32* %dst, i32* %src, i64 %n) optsize {\n"
                "entry:\n"
                "  %is_not_equal = icmp ne i32* %dst, %src\n"
                "  br i1 %is_not_equal, label %memcpy, label %exit\n"
                "memcpy:\n"
                "  call void "
                "@llvm.memcpy.element.unordered.atomic.p0i32.p0i32.i64(i32* "
                "%dst, i32* %src, "
                "i64 %n, i32 4)\n"
                "  br label %exit\n"
                "exit:\n"
                "  ret void\n"
                "}\n");
  Checkpoint Chkpnt = M->getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  FunctionPassManager FPM;
  FPM.addPass(ForwardingPass(
      [=](Function &F, FunctionAnalysisManager &FAM) -> PreservedAnalyses {
        TargetTransformInfo TTI(M->getDataLayout());
        auto *MemCpyBB = getBasicBlockByName(F, "memcpy");
        Instruction *Inst = &MemCpyBB->front();
        assert(isa<AtomicMemCpyInst>(Inst) &&
               "Expecting llvm.memcpy.p0i8.i64 instructon");
        AtomicMemCpyInst *MemCpyI = cast<AtomicMemCpyInst>(Inst);
        auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
        expandAtomicMemCpyAsLoop(MemCpyI, TTI, &SE);
        auto *CopyLoopBB = getBasicBlockByName(F, "loop-memcpy-expansion");
        Instruction *LoadInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Load, 1);
        EXPECT_TRUE(LoadInst->isAtomic());
        EXPECT_NE(LoadInst->getMetadata(LLVMContext::MD_alias_scope), nullptr);
        Instruction *StoreInst =
            getInstructionByOpcode(*CopyLoopBB, Instruction::Store, 1);
        EXPECT_TRUE(StoreInst->isAtomic());
        EXPECT_NE(StoreInst->getMetadata(LLVMContext::MD_noalias), nullptr);
        return PreservedAnalyses::none();
      }));
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  MPM.run(*M, MAM);
  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, SSAUpdaterBulk_SimpleMerge) {
  SSAUpdaterBulk Updater;
  LLVMContext C;
  Module M("SSAUpdaterTest", C);
  IRBuilder<> B(C);
  Type *I32Ty = B.getInt32Ty();
  auto *F = Function::Create(FunctionType::get(B.getVoidTy(), {I32Ty}, false),
                             GlobalValue::ExternalLinkage, "F", &M);

  // Generate a simple program:
  //   if:
  //     br i1 true, label %true, label %false
  //   true:
  //     %1 = add i32 %0, 1
  //     %2 = sub i32 %0, 2
  //     br label %merge
  //   false:
  //     %3 = add i32 %0, 3
  //     %4 = sub i32 %0, 4
  //     br label %merge
  //   merge:
  //     %5 = add i32 %1, 5
  //     %6 = add i32 %3, 6
  //     %7 = add i32 %2, %4
  //     %8 = sub i32 %2, %4
  Argument *FirstArg = &*(F->arg_begin());
  BasicBlock *IfBB = BasicBlock::Create(C, "if", F);
  BasicBlock *TrueBB = BasicBlock::Create(C, "true", F);
  BasicBlock *FalseBB = BasicBlock::Create(C, "false", F);
  BasicBlock *MergeBB = BasicBlock::Create(C, "merge", F);

  B.SetInsertPoint(IfBB);
  B.CreateCondBr(B.getTrue(), TrueBB, FalseBB);

  B.SetInsertPoint(TrueBB);
  Value *AddOp1 = B.CreateAdd(FirstArg, ConstantInt::get(I32Ty, 1));
  Value *SubOp1 = B.CreateSub(FirstArg, ConstantInt::get(I32Ty, 2));
  B.CreateBr(MergeBB);

  B.SetInsertPoint(FalseBB);
  Value *AddOp2 = B.CreateAdd(FirstArg, ConstantInt::get(I32Ty, 3));
  Value *SubOp2 = B.CreateSub(FirstArg, ConstantInt::get(I32Ty, 4));
  B.CreateBr(MergeBB);

  B.SetInsertPoint(MergeBB, MergeBB->begin());
  auto *I1 = cast<Instruction>(B.CreateAdd(AddOp1, ConstantInt::get(I32Ty, 5)));
  auto *I2 = cast<Instruction>(B.CreateAdd(AddOp2, ConstantInt::get(I32Ty, 6)));
  auto *I3 = cast<Instruction>(B.CreateAdd(SubOp1, SubOp2));
  auto *I4 = cast<Instruction>(B.CreateSub(SubOp1, SubOp2));
  (void)I4;

  // Now rewrite uses in instructions %5, %6, %7. They need to use a phi, which
  // SSAUpdater should insert into %merge.
  // Intentionally don't touch %8 to see that SSAUpdater only changes
  // instructions that were explicitly specified.
  unsigned VarNum = Updater.AddVariable("a", I32Ty);
  Updater.AddAvailableValue(VarNum, TrueBB, AddOp1);
  Updater.AddAvailableValue(VarNum, FalseBB, AddOp2);
  Updater.AddUse(VarNum, &I1->getOperandUse(0));
  Updater.AddUse(VarNum, &I2->getOperandUse(0));

  VarNum = Updater.AddVariable("b", I32Ty);
  Updater.AddAvailableValue(VarNum, TrueBB, SubOp1);
  Updater.AddAvailableValue(VarNum, FalseBB, SubOp2);
  Updater.AddUse(VarNum, &I3->getOperandUse(0));
  Updater.AddUse(VarNum, &I3->getOperandUse(1));

  Checkpoint Chkpnt = M.getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  DominatorTree DT(*F);
  Updater.RewriteAllUses(&DT);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

TEST(CheckpointTest, SSAUpdaterBulk_Irreducible) {
  SSAUpdaterBulk Updater;
  LLVMContext C;
  Module M("SSAUpdaterTest", C);
  IRBuilder<> B(C);
  Type *I32Ty = B.getInt32Ty();
  auto *F = Function::Create(FunctionType::get(B.getVoidTy(), {I32Ty}, false),
                             GlobalValue::ExternalLinkage, "F", &M);

  // Generate a small program with a multi-entry loop:
  //     if:
  //       %1 = add i32 %0, 1
  //       br i1 true, label %loopmain, label %loopstart
  //
  //     loopstart:
  //       %2 = add i32 %0, 2
  //       br label %loopmain
  //
  //     loopmain:
  //       %3 = add i32 %1, 3
  //       br i1 true, label %loopstart, label %afterloop
  //
  //     afterloop:
  //       %4 = add i32 %2, 4
  //       ret i32 %0
  Argument *FirstArg = &*F->arg_begin();
  BasicBlock *IfBB = BasicBlock::Create(C, "if", F);
  BasicBlock *LoopStartBB = BasicBlock::Create(C, "loopstart", F);
  BasicBlock *LoopMainBB = BasicBlock::Create(C, "loopmain", F);
  BasicBlock *AfterLoopBB = BasicBlock::Create(C, "afterloop", F);

  B.SetInsertPoint(IfBB);
  Value *AddOp1 = B.CreateAdd(FirstArg, ConstantInt::get(I32Ty, 1));
  B.CreateCondBr(B.getTrue(), LoopMainBB, LoopStartBB);

  B.SetInsertPoint(LoopStartBB);
  Value *AddOp2 = B.CreateAdd(FirstArg, ConstantInt::get(I32Ty, 2));
  B.CreateBr(LoopMainBB);

  B.SetInsertPoint(LoopMainBB);
  auto *I1 = cast<Instruction>(B.CreateAdd(AddOp1, ConstantInt::get(I32Ty, 3)));
  B.CreateCondBr(B.getTrue(), LoopStartBB, AfterLoopBB);

  B.SetInsertPoint(AfterLoopBB);
  auto *I2 = cast<Instruction>(B.CreateAdd(AddOp2, ConstantInt::get(I32Ty, 4)));
  ReturnInst *Return = B.CreateRet(FirstArg);

  Checkpoint Chkpnt = M.getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  // Now rewrite uses in instructions %3, %4, and 'ret i32 %0'. Only %4 needs a
  // new phi, others should be able to work with existing values.
  // The phi for %4 should be inserted into LoopMainBB and should look like
  // this:
  //   %b = phi i32 [ %2, %loopstart ], [ undef, %if ]
  // No other rewrites should be made.

  // Add use in %3.
  unsigned VarNum = Updater.AddVariable("c", I32Ty);
  Updater.AddAvailableValue(VarNum, IfBB, AddOp1);
  Updater.AddUse(VarNum, &I1->getOperandUse(0));

  // Add use in %4.
  VarNum = Updater.AddVariable("b", I32Ty);
  Updater.AddAvailableValue(VarNum, LoopStartBB, AddOp2);
  Updater.AddUse(VarNum, &I2->getOperandUse(0));

  // Add use in the return instruction.
  VarNum = Updater.AddVariable("a", I32Ty);
  Updater.AddAvailableValue(VarNum, &F->getEntryBlock(), FirstArg);
  Updater.AddUse(VarNum, &Return->getOperandUse(0));

  // Save all inserted phis into a vector.
  SmallVector<PHINode *, 8> Inserted;
  DominatorTree DT(*F);
  Updater.RewriteAllUses(&DT, &Inserted);

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

// We use this fixture to ensure that we clean up ScalarEvolution before
// deleting the PassManager.
class CheckpointTest_ScalarEvolutionExpanderTest : public testing::Test {
protected:
  LLVMContext Context;
  Module M;
  TargetLibraryInfoImpl TLII;
  TargetLibraryInfo TLI;

  std::unique_ptr<AssumptionCache> AC;
  std::unique_ptr<DominatorTree> DT;
  std::unique_ptr<LoopInfo> LI;

  CheckpointTest_ScalarEvolutionExpanderTest()
      : M("", Context), TLII(), TLI(TLII) {}

  ScalarEvolution buildSE(Function &F) {
    AC.reset(new AssumptionCache(F));
    DT.reset(new DominatorTree(F));
    LI.reset(new LoopInfo(*DT));
    return ScalarEvolution(F, TLI, *AC, *DT, *LI);
  }

  void runWithSE(
      Module &M, StringRef FuncName,
      function_ref<void(Function &F, LoopInfo &LI, ScalarEvolution &SE)> Test) {
    auto *F = M.getFunction(FuncName);
    ASSERT_NE(F, nullptr) << "Could not find " << FuncName;
    ScalarEvolution SE = buildSE(*F);
    Test(*F, *LI, SE);
  }
};

TEST_F(CheckpointTest_ScalarEvolutionExpanderTest, SCEVZeroExtendExprNonIntegral) {
  /*
   * Create the following code:
   * func(i64 addrspace(10)* %arg)
   * top:
   *  br label %L.ph
   * L.ph:
   *  br label %L
   * L:
   *  %phi = phi i64 [i64 0, %L.ph], [ %add, %L2 ]
   *  %add = add i64 %phi2, 1
   *  br i1 undef, label %post, label %L2
   * post:
   *  %gepbase = getelementptr i64 addrspace(10)* %arg, i64 1
   *  #= %gep = getelementptr i64 addrspace(10)* %gepbase, i64 %add =#
   *  ret void
   *
   * We will create the appropriate SCEV expression for %gep and expand it,
   * then check that no inttoptr/ptrtoint instructions got inserted.
   */

  // Create a module with non-integral pointers in it's datalayout
  Module NIM("nonintegral", Context);
  std::string DataLayout = M.getDataLayoutStr();
  if (!DataLayout.empty())
    DataLayout += "-";
  DataLayout += "ni:10";
  NIM.setDataLayout(DataLayout);

  Type *T_int1 = Type::getInt1Ty(Context);
  Type *T_int64 = Type::getInt64Ty(Context);
  Type *T_pint64 = T_int64->getPointerTo(10);

  FunctionType *FTy =
      FunctionType::get(Type::getVoidTy(Context), {T_pint64}, false);
  Function *F = Function::Create(FTy, Function::ExternalLinkage, "foo", NIM);

  Argument *Arg = &*F->arg_begin();

  BasicBlock *Top = BasicBlock::Create(Context, "top", F);
  BasicBlock *LPh = BasicBlock::Create(Context, "L.ph", F);
  BasicBlock *L = BasicBlock::Create(Context, "L", F);
  BasicBlock *Post = BasicBlock::Create(Context, "post", F);

  IRBuilder<> Builder(Top);
  Builder.CreateBr(LPh);

  Builder.SetInsertPoint(LPh);
  Builder.CreateBr(L);

  Builder.SetInsertPoint(L);
  PHINode *Phi = Builder.CreatePHI(T_int64, 2);
  Value *Add = Builder.CreateAdd(Phi, ConstantInt::get(T_int64, 1), "add");
  Builder.CreateCondBr(UndefValue::get(T_int1), L, Post);
  Phi->addIncoming(ConstantInt::get(T_int64, 0), LPh);
  Phi->addIncoming(Add, L);

  Builder.SetInsertPoint(Post);
  Value *GepBase =
      Builder.CreateGEP(T_int64, Arg, ConstantInt::get(T_int64, 1));
  Instruction *Ret = Builder.CreateRetVoid();
  Checkpoint Chkpnt = M.getContext().getCheckpoint(/*RunVerifier=*/true);
  Chkpnt.save();

  {
    ScalarEvolution SE = buildSE(*F);
    auto *AddRec =
        SE.getAddRecExpr(SE.getUnknown(GepBase), SE.getConstant(T_int64, 1),
                         LI->getLoopFor(L), SCEV::FlagNUW);

    SCEVExpander Exp(SE, NIM.getDataLayout(), "expander");
    Exp.disableCanonicalMode();
    Exp.expandCodeFor(AddRec, T_pint64, Ret);
  }

  EXPECT_FALSE(Chkpnt.empty());
  Chkpnt.rollback();
}

} // namespace
} // namespace llvm
