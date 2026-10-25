; RUN: opt -passes=loop-unroll-full -S < %s | FileCheck %s
; RUN: opt -passes=loop-unroll -unroll-runtime -S < %s | FileCheck %s
; RUN: opt -passes=loop-unroll -unroll-peel-count=2 -S < %s | FileCheck %s
; RUN: opt -passes='loop(loop-rotate)' -S < %s | FileCheck %s
; RUN: opt -passes='simple-loop-unswitch<nontrivial>' -S < %s | FileCheck %s
; RUN: opt -passes=irce -S < %s | FileCheck %s

; Loops containing an indirectswitch can't be unrolled, peeled, rotated
; (header duplication) or unswitched, as the indirectswitch can't be
; duplicated.

declare void @use(i32)

; CHECK-LABEL: @f(
; CHECK: indirectswitch
; CHECK-NOT: indirectswitch
define void @f(ptr %t, i32 %n, i1 %c) {
entry:
  br label %header

header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %tgt = load ptr, ptr %t, align 8
  indirectswitch ptr %tgt, [ ptr edgeaddress(@f, 0), label %x
                             ptr edgeaddress(@f, 1), label %y ]

x:
  br i1 %c, label %latch, label %y

y:
  call void @use(i32 %i)
  br label %latch

latch:
  %i.next = add nuw nsw i32 %i, 1
  %exitcond = icmp eq i32 %i.next, 4
  br i1 %exitcond, label %exit, label %header

exit:
  ret void
}
