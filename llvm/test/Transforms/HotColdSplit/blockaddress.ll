; RUN: opt -passes=hotcoldsplit -hotcoldsplit-threshold=-1 -S < %s | FileCheck %s

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

declare void @sink() cold
declare void @use(ptr)

; A blockaddress is data-only, so cold blocks whose address is taken can be
; outlined. The blockaddress then refers to the block in the outlined function.
@tbl = global [2 x ptr] [ptr blockaddress(@address_taken, %cold), ptr blockaddress(@address_taken, %exit)]

; CHECK: @tbl = global [2 x ptr] [ptr blockaddress(@address_taken.cold.1, %cold), ptr blockaddress(@address_taken, %exit)]

; CHECK-LABEL: define void @address_taken(
; CHECK: call void @address_taken.cold.1()
define void @address_taken(i1 %c) {
entry:
  br i1 %c, label %cold, label %exit

cold:
  call void @sink()
  call void @sink()
  br label %exit

exit:
  ret void
}

; Cold code using a blockaddress (of a block that isn't outlined) can be
; outlined as well.
; CHECK-LABEL: define void @uses_blockaddress(
; CHECK: call void @uses_blockaddress.cold.1()
define void @uses_blockaddress(i1 %c) {
entry:
  br i1 %c, label %cold, label %exit

cold:
  call void @sink()
  call void @use(ptr blockaddress(@uses_blockaddress, %exit))
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: define internal void @address_taken.cold.1(
; CHECK:       cold:
; CHECK-NEXT:    call void @sink()

; CHECK-LABEL: define internal void @uses_blockaddress.cold.1(
; CHECK:         call void @use(ptr blockaddress(@uses_blockaddress, %exit))
