; RUN: opt -passes=strip -S < %s | FileCheck %s
; PR10286

@main_addrs = constant [2 x ptr] [ptr blockaddress(@f, %FOO), ptr blockaddress(@f, %BAR)]
; CHECK: @main_addrs = constant [2 x ptr] [ptr blockaddress(@f, %2), ptr blockaddress(@f, %3)]

declare void @foo() nounwind
declare void @bar() nounwind

define void @f(i1 %c) nounwind uwtable ssp {
entry:
  br i1 %c, label %FOO, label %BAR

  ; CHECK: br i1 %0, label %2, label %3

FOO:
  call void @foo()
  ret void

BAR:
  call void @bar()
  ret void
}
