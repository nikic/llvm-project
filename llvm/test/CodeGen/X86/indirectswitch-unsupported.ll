; RUN: split-file %s %t
; RUN: not llc -mtriple=x86_64 < %t/inst.ll 2>&1 | FileCheck %s --check-prefix=INST
; RUN: not llc -mtriple=x86_64 -O0 -fast-isel < %t/inst.ll 2>&1 | FileCheck %s --check-prefix=INST
; RUN: not llc -mtriple=x86_64 -global-isel -global-isel-abort=1 < %t/inst.ll 2>&1 | FileCheck %s --check-prefix=INST
; RUN: not llc -mtriple=x86_64 < %t/const.ll 2>&1 | FileCheck %s --check-prefix=CONST
; RUN: not llc -mtriple=x86_64 -global-isel -global-isel-abort=1 < %t/const.ll 2>&1 | FileCheck %s --check-prefix=CONST
; RUN: not llc -mtriple=x86_64 < %t/global.ll 2>&1 | FileCheck %s --check-prefix=CONST

; Lowering of indirectswitch and edgeaddress is not implemented yet.

; INST: LLVM ERROR: indirectswitch is not yet supported
; CONST: LLVM ERROR: edgeaddress is not yet supported

;--- inst.ll
define void @f(ptr %p) {
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a ]
a:
  ret void
}

;--- const.ll
define ptr @f() {
  ret ptr edgeaddress(@f, 0)
}

;--- global.ll
@g = global ptr edgeaddress(@f, 0)

define void @f() {
  ret void
}
