; RUN: opt -module-summary %s -o %t1.bc
; RUN: opt -module-summary %p/Inputs/globals-import-edgeaddr.ll -o %t2.bc
; RUN: llvm-lto2 run -save-temps %t1.bc -r=%t1.bc,foo,l -r=%t1.bc,bar,l -r=%t1.bc,baz,l -r=%t1.bc,main,pl %t2.bc -r=%t2.bc,foo,pl -r=%t2.bc,bar,pl -r=%t2.bc,baz,pl -o %t3
; RUN: llvm-dis %t3.1.3.import.bc -o - | FileCheck %s

; Verify that we haven't imported GV containing edgeaddress
; CHECK: @label_addr.llvm.0 = external hidden constant
; Verify that bar is not imported since it contains an indirectswitch
; CHECK-DAG: declare ptr @bar(ptr)
; Verify that baz is not imported since it references an edgeaddress
; CHECK-DAG: declare ptr @baz()
; Verify that foo is imported
; CHECK-DAG: available_externally ptr @foo

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare dso_local ptr @foo();
declare dso_local ptr @bar(ptr);
declare dso_local ptr @baz();

define dso_local i32 @main() {
  %p1 = call ptr @foo()
  %p2 = call ptr @bar(ptr null)
  %p3 = call ptr @baz()
  %v1 = ptrtoint ptr %p1 to i32
  %v2 = ptrtoint ptr %p2 to i32
  %v3 = ptrtoint ptr %p3 to i32
  %v4 = add i32 %v1, %v2
  %v5 = add i32 %v4, %v3
  ret i32 %v5
}
