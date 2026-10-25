; RUN: llvm-as < %s | llvm-dis | FileCheck %s
; RUN: llvm-as < %s | llvm-dis | llvm-as | llvm-dis | FileCheck %s
; RUN: verify-uselistorder %s

target datalayout = "P1"

; Forward references to functions in global initializers.
; CHECK: @table = constant [3 x ptr addrspace(1)] [ptr addrspace(1) edgeaddress(@f, 0), ptr addrspace(1) edgeaddress(@f, 1), ptr addrspace(1) edgeaddress(@f, 42)]
@table = constant [3 x ptr addrspace(1)] [ptr addrspace(1) edgeaddress(@f, 0), ptr addrspace(1) edgeaddress(@f, 1), ptr addrspace(1) edgeaddress(@f, 42)]

; CHECK: @diff = constant i64 sub (i64 ptrtoint (ptr addrspace(1) edgeaddress(@f, 1) to i64), i64 ptrtoint (ptr addrspace(1) edgeaddress(@f, 0) to i64))
@diff = constant i64 sub (i64 ptrtoint (ptr addrspace(1) edgeaddress(@f, 1) to i64), i64 ptrtoint (ptr addrspace(1) edgeaddress(@f, 0) to i64))

; An edgeaddress that is not used by any indirectswitch.
; CHECK: @unused = global ptr addrspace(1) edgeaddress(@f, 100)
@unused = global ptr addrspace(1) edgeaddress(@f, 100)

; Numbered function.
; CHECK: @numbered = global ptr addrspace(1) edgeaddress(@0, 3)
@numbered = global ptr addrspace(1) edgeaddress(@0, 3)

; Forward reference from another function's body.
define ptr addrspace(1) @before() addrspace(1) {
; CHECK-LABEL: define ptr addrspace(1) @before()
; CHECK: ret ptr addrspace(1) edgeaddress(@f, 1)
  ret ptr addrspace(1) edgeaddress(@f, 1)
}

define void @f(ptr addrspace(1) %p) addrspace(1) {
; CHECK-LABEL: define void @f(
; CHECK:      indirectswitch ptr addrspace(1) %addr, [
; CHECK-NEXT:   ptr addrspace(1) edgeaddress(@f, 0), label %a
; CHECK-NEXT:   ptr addrspace(1) edgeaddress(@f, 1), label %b
; CHECK-NEXT:   ptr addrspace(1) edgeaddress(@f, 42), label %b
; CHECK-NEXT:   ptr addrspace(1) edgeaddress(@f, 7), label %c
; CHECK-NEXT: ]
entry:
  br label %dispatch

dispatch:
  %addr = phi ptr addrspace(1) [ %p, %entry ], [ edgeaddress(@f, 1), %a ], [ %p, %b ]
  indirectswitch ptr addrspace(1) %addr, [ ptr addrspace(1) edgeaddress(@f, 0), label %a
                                           ptr addrspace(1) edgeaddress(@f, 1), label %b
                                           ptr addrspace(1) edgeaddress(@f, 42), label %b
                                           ptr addrspace(1) edgeaddress(@f, 7), label %c ]

a:
  br label %dispatch

b:
  br label %dispatch

c:
  ret void
}

; Edge addresses of a function that has already been defined.
define ptr addrspace(1) @after() addrspace(1) {
; CHECK-LABEL: define ptr addrspace(1) @after()
; CHECK: ret ptr addrspace(1) edgeaddress(@f, 7)
  ret ptr addrspace(1) edgeaddress(@f, 7)
}

; An indirectswitch without cases.
define void @no_cases(ptr addrspace(1) %p) addrspace(1) {
; CHECK-LABEL: define void @no_cases(
; CHECK:      indirectswitch ptr addrspace(1) %p, [
; CHECK-NEXT: ]
  indirectswitch ptr addrspace(1) %p, []
}

; Branch weights.
define void @prof(ptr addrspace(1) %p) addrspace(1) {
; CHECK-LABEL: define void @prof(
; CHECK: indirectswitch ptr addrspace(1) %p, [
; CHECK: ], !prof !0
  indirectswitch ptr addrspace(1) %p, [ ptr addrspace(1) edgeaddress(@prof, 0), label %a
                                        ptr addrspace(1) edgeaddress(@prof, 1), label %b ], !prof !0
a:
  ret void
b:
  ret void
}

define void @0(ptr addrspace(1) %p) addrspace(1) {
; CHECK-LABEL: define void @0(
; CHECK: indirectswitch ptr addrspace(1) %p, [
; CHECK-NEXT: ptr addrspace(1) edgeaddress(@0, 3), label %a
  indirectswitch ptr addrspace(1) %p, [ ptr addrspace(1) edgeaddress(@0, 3), label %a ]
a:
  ret void
}

!0 = !{!"branch_weights", i32 1, i32 2}
