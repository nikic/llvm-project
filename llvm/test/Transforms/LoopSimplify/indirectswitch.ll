; RUN: opt -passes=loop-simplify -S < %s | FileCheck %s

; Unlike for indirectbr, LoopSimplify can split the edges from an
; indirectswitch to create a preheader and dedicated exit blocks.

declare void @use(i32)

; CHECK-LABEL: define void @f(
; CHECK:       entry:
; CHECK-NEXT:    indirectswitch ptr %p, [
; CHECK-NEXT:      ptr edgeaddress(@f, 0), label %header.preheader
; CHECK-NEXT:      ptr edgeaddress(@f, 1), label %exit
; CHECK-NEXT:    ]
; CHECK:       header.preheader:
; CHECK-NEXT:    br label %header
; CHECK:       header:
; CHECK:         indirectswitch ptr %q, [
; CHECK-NEXT:      ptr edgeaddress(@f, 2), label %header
; CHECK-NEXT:      ptr edgeaddress(@f, 3), label %exit.loopexit
; CHECK-NEXT:    ]
; CHECK:       exit.loopexit:
; CHECK-NEXT:    br label %exit
define void @f(ptr %p, ptr %q) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %header
                           ptr edgeaddress(@f, 1), label %exit ]

header:
  call void @use(i32 0)
  indirectswitch ptr %q, [ ptr edgeaddress(@f, 2), label %header
                           ptr edgeaddress(@f, 3), label %exit ]

exit:
  ret void
}
