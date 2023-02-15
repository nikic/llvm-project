//===- llvm/unittest/IR/GlobalValueTest.cpp - GlobalValue unit tests ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/GlobalValue.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/SourceMgr.h"
#include "gtest/gtest.h"
#include <memory>

namespace llvm {
namespace {

static std::unique_ptr<Module> parseIR(LLVMContext &C, const char *IR) {
  SMDiagnostic Err;
  std::unique_ptr<Module> Mod = parseAssemblyString(IR, Err, C);
  if (!Mod)
    Err.print("GlobalValueTest", errs());
  return Mod;
}

TEST(GlobalValueTest, BitFields) {
  LLVMContext C;
  std::unique_ptr<Module> M = parseIR(C, R"(
@GV = external global i32
)");
  GlobalVariable *GV = M->getGlobalVariable("GV");
  for (auto Lnk :
       {GlobalValue::ExternalLinkage, GlobalValue::AvailableExternallyLinkage,
        GlobalValue::LinkOnceAnyLinkage, GlobalValue::LinkOnceODRLinkage,
        GlobalValue::WeakAnyLinkage, GlobalValue::WeakODRLinkage,
        GlobalValue::AppendingLinkage, GlobalValue::InternalLinkage,
        GlobalValue::PrivateLinkage, GlobalValue::ExternalWeakLinkage,
        GlobalValue::CommonLinkage}) {
    GV->setLinkage(Lnk);
    EXPECT_EQ(GV->getLinkage(), Lnk);
  }
  for (auto Vis :
       {GlobalValue::DefaultVisibility, GlobalValue::HiddenVisibility,
        GlobalValue::ProtectedVisibility}) {
    GV->setVisibility(Vis);
    EXPECT_EQ(GV->getVisibility(), Vis);
  }
  for (auto Addr :
       {GlobalValue::UnnamedAddr::None, GlobalValue::UnnamedAddr::Local,
        GlobalValue::UnnamedAddr::Global}) {
    GV->setUnnamedAddr(Addr);
    EXPECT_EQ(GV->getUnnamedAddr(), Addr);
  }
  for (auto Class :
       {GlobalValue::DefaultStorageClass, GlobalValue::DLLImportStorageClass,
        GlobalValue::DLLExportStorageClass}) {
    GV->setDLLStorageClass(Class);
    EXPECT_EQ(GV->getDLLStorageClass(), Class);
  }
  for (auto Mode :
       {GlobalValue::NotThreadLocal, GlobalValue::GeneralDynamicTLSModel,
        GlobalValue::LocalDynamicTLSModel, GlobalValue::InitialExecTLSModel,
        GlobalValue::LocalExecTLSModel}) {
    GV->setThreadLocalMode(Mode);
    EXPECT_EQ(GV->getThreadLocalMode(), Mode);
  }
  for (auto Local : {true, false}) {
    GV->setDSOLocal(Local);
    EXPECT_EQ(GV->isDSOLocal(), Local);
  }
  GV->setPartition("Partition");
  EXPECT_TRUE(GV->hasPartition());

  GlobalValue::SanitizerMetadata SM;
  SM.NoAddress = 1;
  GV->setSanitizerMetadata(SM);
  EXPECT_TRUE(GV->hasSanitizerMetadata());
}

} // namespace
} // namespace llvm
