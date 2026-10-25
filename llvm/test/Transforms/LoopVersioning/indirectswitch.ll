; RUN: opt -passes=loop-versioning -S < %s | FileCheck %s
; RUN: opt -passes=loop-distribute -enable-loop-distribute -S < %s | FileCheck %s
; RUN: opt -passes=loop-load-elim -S < %s | FileCheck %s

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

; Do not version or distribute this loop, because the indirectswitch can't be
; duplicated.

; CHECK-LABEL: @f(
; CHECK: indirectswitch
; CHECK-NOT: indirectswitch
define void @f(ptr %a, ptr %b, ptr %c, ptr %d, ptr %t) {
entry:
  br label %for.body

for.body:
  %ind = phi i64 [ 0, %entry ], [ %add, %latch ]
  %arrayidxA = getelementptr inbounds i32, ptr %a, i64 %ind
  %loadA = load i32, ptr %arrayidxA, align 4
  %arrayidxB = getelementptr inbounds i32, ptr %b, i64 %ind
  %loadB = load i32, ptr %arrayidxB, align 4
  %mulA = mul i32 %loadB, %loadA
  %add = add nuw nsw i64 %ind, 1
  %arrayidxA_plus_4 = getelementptr inbounds i32, ptr %a, i64 %add
  store i32 %mulA, ptr %arrayidxA_plus_4, align 4
  %arrayidxD = getelementptr inbounds i32, ptr %d, i64 %ind
  %loadD = load i32, ptr %arrayidxD, align 4
  %arrayidxC = getelementptr inbounds i32, ptr %c, i64 %ind
  %tgt = load ptr, ptr %t, align 8
  indirectswitch ptr %tgt, [ ptr edgeaddress(@f, 0), label %x
                             ptr edgeaddress(@f, 1), label %y ]

x:
  br label %latch

y:
  br label %latch

latch:
  %v = phi i32 [ 1, %x ], [ 2, %y ]
  %mulC = mul i32 %loadD, %v
  store i32 %mulC, ptr %arrayidxC, align 4
  %exitcond = icmp eq i64 %add, 20
  br i1 %exitcond, label %for.end, label %for.body

for.end:
  ret void
}
