; facebook begin T130678741
; RUN: opt < %s -aa-pipeline=separatestorage-aa,basic-aa -gvn -enable-separate-storage-aa -S | FileCheck %s

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
declare void @llvm.experimental.separate.storage(ptr, ptr)

; Test basic queries.

; CHECK-LAEBL: @simple_no
; CHECK: ret i8 %loadofstore
define i8 @simple_no(ptr %p1, ptr %p2) {
entry:
  store i8 0, ptr %p1
  store i8 1, ptr %p2
  %loadofstore = load i8, ptr %p1
  ret i8 %loadofstore
}

; CHECK-LABEL: @simple_yes
; CHECK: ret i8 0
define i8 @simple_yes(ptr %p1, ptr %p2) {
entry:
  call void @llvm.experimental.separate.storage(ptr %p1, ptr %p2)
  store i8 0, ptr %p1
  store i8 1, ptr %p2
  %loadofstore = load i8, ptr %p1
  ret i8 %loadofstore
}

; CHECK-LABEL: @ptr_to_ptr_no
; CHECK: ret i8 %loadofstore
define i8 @ptr_to_ptr_no(ptr %pp) {
entry:
  %p_base = load ptr, ptr %pp
  store i8 0, ptr %p_base
  %p_base2 = load ptr, ptr %pp
  %loadofstore = load i8, ptr %p_base2
  ret i8 %loadofstore
}

; CHECK-LABEL: @ptr_to_ptr_yes
; CHECK: ret i8 0
define i8 @ptr_to_ptr_yes(ptr %pp) {
entry:
  %p_base = load ptr, ptr %pp
  call void @llvm.experimental.separate.storage(ptr %p_base, ptr %pp)
  store i8 0, ptr %p_base
  %p_base2 = load ptr, ptr %pp
  %loadofstore = load i8, ptr %p_base2
  ret i8 %loadofstore
}

; The analysis should only kick in if executed (or will be executed) at the
; given program point.

; CHECK-LABEL: @flow_sensitive
; CHECK: %loadofstore = phi i8 [ %loadofstore_true, %true_branch ], [ 33, %false_branch ]
define i8 @flow_sensitive(ptr %p1, ptr %p2, i1 %cond) {
entry:
  br i1 %cond, label %true_branch, label %false_branch

true_branch:
  store i8 11, ptr %p1
  store i8 22, ptr %p2
  %loadofstore_true = load i8, ptr %p1
  br label %endif

false_branch:
  call void @llvm.experimental.separate.storage(ptr %p1, ptr %p2)
  store i8 33, ptr %p1
  store i8 44, ptr %p2
  %loadofstore_false = load i8, ptr %p1
  br label %endif

endif:
  %loadofstore = phi i8 [ %loadofstore_true, %true_branch ], [ %loadofstore_false, %false_branch ]
  ret i8 %loadofstore
}

; CHECK-LABEL: @flow_sensitive_with_dominator
; CHECK: %loadofstore = phi i8 [ 11, %true_branch ], [ 33, %false_branch ]
define i8 @flow_sensitive_with_dominator(ptr %p1, ptr %p2, i1 %cond) {
entry:
  call void @llvm.experimental.separate.storage(ptr %p1, ptr %p2)
  br i1 %cond, label %true_branch, label %false_branch

true_branch:
  store i8 11, ptr %p1
  store i8 22, ptr %p2
  %loadofstore_true = load i8, ptr %p1
  br label %endif

false_branch:
  store i8 33, ptr %p1
  store i8 44, ptr %p2
  %loadofstore_false = load i8, ptr %p1
  br label %endif

endif:
  %loadofstore = phi i8 [ %loadofstore_true, %true_branch ], [ %loadofstore_false, %false_branch ]
  ret i8 %loadofstore
}

; Hints are relative to entire regions of storage, not particular pointers
; inside them. We should know that the whole ranges are disjoint given hints at
; offsets.

; CHECK-LABEL: @offset_agnostic
; CHECK: ret i8 0
define i8 @offset_agnostic(ptr %p1, ptr %p2) {
  %access1 = getelementptr inbounds i8, ptr %p1, i64 12
  %access2 = getelementptr inbounds i8, ptr %p2, i64 34

  %hint1 = getelementptr inbounds i8, ptr %p1, i64 56
  %hint2 = getelementptr inbounds i8, ptr %p2, i64 78
  call void @llvm.experimental.separate.storage(ptr %hint1, ptr %hint2)

  store i8 0, ptr %access1
  store i8 1, ptr %access2
  %loadofstore = load i8, ptr %access1
  ret i8 %loadofstore
}
; facebook end T130678741
