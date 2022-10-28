; RUN: opt < %s -passes=aa-eval -print-all-alias-modref-info 2>&1 | FileCheck %s

@c = constant [8 x i32] zeroinitializer

declare void @dummy()

; CHECK-LABEL: Function: basic
; CHECK: NoModRef: Ptr: i32* @c	<->  call void @dummy()
define void @basic(ptr %p) {
  call void @dummy()
  load i32, ptr @c
  ret void
}

; CHECK-LABEL: Function: recphi
; CHECK: NoModRef: Ptr: i32* %p	<->  call void @dummy()
define void @recphi() {
entry:
  br label %loop

loop:
  %p = phi ptr [ @c, %entry ], [ %p.next, %loop ]
  call void @dummy()
  load i32, ptr %p
  %p.next = getelementptr i32, ptr %p, i64 1
  %c = icmp ne ptr %p.next, getelementptr (i32, ptr @c, i64 8)
  br i1 %c, label %loop, label %exit

exit:
  ret void
}
