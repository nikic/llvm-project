; RUN: split-file --leading-lines %s %t
; RUN: not llvm-as < %t/zero.ll 2>&1 | FileCheck %s --check-prefix=ZERO
; RUN: not llvm-as < %t/missing-rparen.ll 2>&1 | FileCheck %s --check-prefix=RPAREN
; RUN: not llvm-as < %t/not-int.ll 2>&1 | FileCheck %s --check-prefix=NOTINT

;--- zero.ll
; ZERO: <stdin>:[[@LINE+1]]:27: error: extension width must be non-zero
define void @f(i8 zeroext(0) %a) {
  ret void
}

;--- missing-rparen.ll
; RPAREN: <stdin>:[[@LINE+1]]:30: error: expected ')' after extension width
define void @f(i8 signext(16 %a) {
  ret void
}

;--- not-int.ll
; NOTINT: <stdin>:[[@LINE+1]]:27: error: expected integer
define void @f(i8 zeroext(x) %a) {
  ret void
}
