; RUN: opt < %s -passes=loop-vectorize -debug-only=loop-vectorize -S -disable-output 2>&1 | FileCheck %s
; REQUIRES: asserts

; Make sure LV legal bails out when there is a non-int, non-ptr phi
; CHECK-LABEL: 'invalid_phi_types'
; CHECK: LV: Not vectorizing: Found a non-int non-pointer PHI.
define i32 @invalid_phi_types() {
entry:
  br label %for.body

for.body:
  %i.02 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %vec.sum.02 = phi <2 x i32> [ zeroinitializer, %entry ], [ <i32 8, i32 8>, %for.body ]
  %inc = add nsw i32 %i.02, 1
  %cmp = icmp slt i32 %inc, 16
  br i1 %cmp, label %for.body, label %for.end

for.end:
  ret i32 0
}

; Make sure LV legal bails out when the pre-header is terminated by an
; indirectswitch instead of an unconditional branch.
; CHECK-LABEL: 'preheader_indirectswitch'
; CHECK: LV: Not vectorizing: Loop doesn't have a legal pre-header.
define void @preheader_indirectswitch(ptr %dst, i64 %n) {
entry:
  indirectswitch ptr edgeaddress(@preheader_indirectswitch, 0), [ ptr edgeaddress(@preheader_indirectswitch, 0), label %loop ]

loop:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %loop ]
  %gep = getelementptr inbounds i8, ptr %dst, i64 %iv
  store i8 0, ptr %gep, align 1
  %iv.next = add nuw nsw i64 %iv, 1
  %ec = icmp eq i64 %iv.next, %n
  br i1 %ec, label %exit, label %loop

exit:
  ret void
}

; Make sure LV legal bails out when the pre-header is terminated by a switch
; with a single destination instead of an unconditional branch.
; CHECK-LABEL: 'preheader_switch'
; CHECK: LV: Not vectorizing: Loop doesn't have a legal pre-header.
define void @preheader_switch(ptr %dst, i64 %n, i32 %x) {
entry:
  switch i32 %x, label %loop []

loop:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %loop ]
  %gep = getelementptr inbounds i8, ptr %dst, i64 %iv
  store i8 0, ptr %gep, align 1
  %iv.next = add nuw nsw i64 %iv, 1
  %ec = icmp eq i64 %iv.next, %n
  br i1 %ec, label %exit, label %loop

exit:
  ret void
}
