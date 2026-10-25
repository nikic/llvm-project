; RUN: split-file %s %t
; RUN: not llvm-as -disable-output %t/other-function.ll 2>&1 | FileCheck %s --check-prefix=OTHER-FN
; RUN: not llvm-as -disable-output %t/multiple-users.ll 2>&1 | FileCheck %s --check-prefix=MULTI
; RUN: not llvm-as -disable-output %t/declaration.ll 2>&1 | FileCheck %s --check-prefix=DECL
; RUN: not llvm-as -disable-output %t/address-space.ll 2>&1 | FileCheck %s --check-prefix=AS
; RUN: llvm-as -disable-output %t/valid.ll

;--- other-function.ll
; OTHER-FN: indirectswitch case value must be an edgeaddress of the parent function
; OTHER-FN-NEXT: indirectswitch ptr %p, [
; OTHER-FN: ptr edgeaddress(@g, 0)
define void @g() {
  ret void
}

define void @f(ptr %p) {
  indirectswitch ptr %p, [ ptr edgeaddress(@g, 0), label %a ]
a:
  ret void
}

;--- multiple-users.ll
; MULTI: edgeaddress used as case value by multiple indirectswitch instructions
define void @f(ptr %p, i1 %c) {
  br i1 %c, label %x, label %y
x:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a ]
y:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 1), label %a
                           ptr edgeaddress(@f, 0), label %a ]
a:
  ret void
}

;--- declaration.ll
; DECL: edgeaddress must refer to a function definition
; DECL-NEXT: ptr edgeaddress(@decl, 0)
@g = global ptr edgeaddress(@decl, 0)
declare void @decl()

;--- address-space.ll
; AS: indirectswitch operand must have the pointer type of the function
define void @f(ptr addrspace(1) %p) {
  indirectswitch ptr addrspace(1) %p, [ ]
}

;--- valid.ll
; Using an edgeaddress as the address operand of another indirectswitch, or in
; another function, is allowed.
define void @f(ptr %p, i1 %c) {
  br i1 %c, label %x, label %y
x:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a ]
y:
  indirectswitch ptr edgeaddress(@f, 0), [ ptr edgeaddress(@f, 1), label %a ]
a:
  ret void
}

define ptr @g() {
  ret ptr edgeaddress(@f, 0)
}
