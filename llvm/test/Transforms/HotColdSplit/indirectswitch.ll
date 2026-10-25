; RUN: opt -passes=hotcoldsplit -hotcoldsplit-threshold=-1 -S < %s | FileCheck %s

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

declare void @sink() cold
declare void @use(ptr)

@tbl = constant [2 x ptr] [ptr edgeaddress(@dest, 0), ptr edgeaddress(@dest, 1)]

; A cold destination of an indirectswitch can be outlined, unlike a cold
; indirectbr destination (which is address-taken).
; CHECK-LABEL: define void @dest(
; CHECK: indirectswitch ptr %p, [
; CHECK-NEXT: ptr edgeaddress(@dest, 0), label %[[COLD:.*]]
; CHECK-NEXT: ptr edgeaddress(@dest, 1), label %exit
; CHECK: [[COLD]]:
; CHECK-NEXT: call void @dest.cold.1(
define void @dest(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@dest, 0), label %cold
                           ptr edgeaddress(@dest, 1), label %exit ]

cold:
  call void @sink()
  call void @sink()
  br label %exit

exit:
  ret void
}

; A cold region containing an indirectswitch can't be outlined.
; CHECK-LABEL: define void @contains_switch(
; CHECK-NOT: call void @contains_switch.cold
; CHECK: indirectswitch
define void @contains_switch(ptr %p, i1 %c) {
entry:
  br i1 %c, label %cold, label %exit

cold:
  call void @sink()
  indirectswitch ptr %p, [ ptr edgeaddress(@contains_switch, 0), label %cold2 ]

cold2:
  call void @sink()
  call void @use(ptr edgeaddress(@contains_switch, 0))
  br label %exit

exit:
  ret void
}

; A cold region using an edgeaddress of its function isn't outlined.
; CHECK-LABEL: define void @uses_edgeaddress(
; CHECK-NOT: call void @uses_edgeaddress.cold
; CHECK: call void @use(ptr edgeaddress(@uses_edgeaddress, 0))
define void @uses_edgeaddress(ptr %p, i1 %c) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@uses_edgeaddress, 0), label %bb ]

bb:
  br i1 %c, label %cold, label %exit

cold:
  call void @sink()
  call void @use(ptr edgeaddress(@uses_edgeaddress, 0))
  br label %exit

exit:
  ret void
}

; CHECK-LABEL: define {{.*}} @dest.cold.1(
; CHECK-NOT: define {{.*}} @contains_switch.cold
; CHECK-NOT: define {{.*}} @uses_edgeaddress.cold
