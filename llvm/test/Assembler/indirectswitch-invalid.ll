; RUN: split-file %s %t
; RUN: not llvm-as -disable-output %t/not-edgeaddress.ll 2>&1 | FileCheck %s --check-prefix=NOT-EA
; RUN: not llvm-as -disable-output %t/duplicate.ll 2>&1 | FileCheck %s --check-prefix=DUP
; RUN: not llvm-as -disable-output %t/not-pointer.ll 2>&1 | FileCheck %s --check-prefix=NOT-PTR
; RUN: not llvm-as -disable-output %t/undefined-fn.ll 2>&1 | FileCheck %s --check-prefix=UNDEF-FN
; RUN: not llvm-as -disable-output %t/not-fn.ll 2>&1 | FileCheck %s --check-prefix=NOT-FN
; RUN: not llvm-as -disable-output %t/fwd-not-fn.ll 2>&1 | FileCheck %s --check-prefix=FWD-NOT-FN
; RUN: not llvm-as -disable-output %t/bad-id.ll 2>&1 | FileCheck %s --check-prefix=BAD-ID
; RUN: not llvm-as -disable-output %t/local.ll 2>&1 | FileCheck %s --check-prefix=LOCAL
; RUN: not llvm-as -disable-output %t/fwd-wrong-as.ll 2>&1 | FileCheck %s --check-prefix=WRONG-AS

;--- not-edgeaddress.ll
; NOT-EA: error: case value is not an edgeaddress
define void @f(ptr %p) {
  indirectswitch ptr %p, [ ptr null, label %a ]
a:
  ret void
}

;--- duplicate.ll
; DUP: error: duplicate case value in indirectswitch
define void @f(ptr %p) {
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 0), label %a ]
a:
  ret void
}

;--- not-pointer.ll
; NOT-PTR: error: indirectswitch address must have pointer type
define void @f(i64 %p) {
  indirectswitch i64 %p, [ ptr edgeaddress(@f, 0), label %a ]
a:
  ret void
}

;--- undefined-fn.ll
; UNDEF-FN: error: expected function name in edgeaddress
@g = global ptr edgeaddress(@undefined, 0)

;--- not-fn.ll
; NOT-FN: error: expected function name in edgeaddress
@x = global i32 0
@g = global ptr edgeaddress(@x, 0)

;--- fwd-not-fn.ll
; FWD-NOT-FN: error: expected function name in edgeaddress
@g = global ptr edgeaddress(@x, 0)
@x = global i32 0

;--- bad-id.ll
; BAD-ID: error: expected integer
@g = global ptr edgeaddress(@f, %x)
define void @f() {
  ret void
}

;--- local.ll
; LOCAL: error: expected function name in edgeaddress
define void @f(ptr %p) {
  ret ptr edgeaddress(%p, 0)
}

;--- fwd-wrong-as.ll
; WRONG-AS: error: edgeaddress type does not match the address space of the function
@g = global ptr addrspace(1) edgeaddress(@f, 0)
define void @f() {
  ret void
}
