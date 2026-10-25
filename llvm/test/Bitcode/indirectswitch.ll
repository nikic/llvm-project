; RUN: llvm-as < %s | llvm-dis | FileCheck %s
; RUN: llvm-as < %s | llvm-bcanalyzer -dump | FileCheck %s --check-prefix=BC
; RUN: verify-uselistorder < %s

; Edge addresses in global initializers are read before the function body.
; CHECK: @table = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 5)]
@table = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 5)]
; CHECK: @unused = global ptr edgeaddress(@f, 9)
@unused = global ptr edgeaddress(@f, 9)
; CHECK: @lazy = global ptr edgeaddress(@lazy_fn, 0)
@lazy = global ptr edgeaddress(@lazy_fn, 0)

; BC: <EDGEADDRESS op0={{[0-9]+}} op1={{[0-9]+}} op2=0/>
; BC: <EDGEADDRESS op0={{[0-9]+}} op1={{[0-9]+}} op2=5/>
; BC: <EDGEADDRESS op0={{[0-9]+}} op1={{[0-9]+}} op2=9/>
; BC: <INST_INDIRECTSWITCH
; BC: <INST_INDIRECTSWITCH

define void @f(ptr %p) {
; CHECK-LABEL: define void @f(
; CHECK:      indirectswitch ptr %p, [
; CHECK-NEXT:   ptr edgeaddress(@f, 0), label %a
; CHECK-NEXT:   ptr edgeaddress(@f, 5), label %b
; CHECK-NEXT:   ptr edgeaddress(@f, 3), label %a
; CHECK-NEXT: ]
; CHECK: store ptr edgeaddress(@f, 3), ptr %p
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 5), label %b
                           ptr edgeaddress(@f, 3), label %a ]
a:
  store ptr edgeaddress(@f, 3), ptr %p
  ret void
b:
  ret void
}

define void @no_cases(ptr %p) {
; CHECK-LABEL: define void @no_cases(
; CHECK:      indirectswitch ptr %p, [
; CHECK-NEXT: ]
  indirectswitch ptr %p, []
}

define void @lazy_fn(ptr %p) {
  indirectswitch ptr %p, [ ptr edgeaddress(@lazy_fn, 0), label %a ]
a:
  ret void
}
