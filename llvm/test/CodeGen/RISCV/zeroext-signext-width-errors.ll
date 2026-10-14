; RUN: not llc -mtriple=riscv32 < %s 2>&1 | FileCheck %s --check-prefix=RV32
; RUN: not llc -mtriple=riscv64 < %s 2>&1 | FileCheck %s --check-prefix=RV64

; An extension width larger than the location of the argument is ill-formed.

; RV32: error: argument extension width 64 exceeds the size of the argument location (32 bits) in function 'call_i32_zext64'
; RV64: error: argument extension width 128 exceeds the size of the argument location (64 bits) in function 'call_i32_zext64'
declare void @take_i32_zext64(i32 zeroext(64), i32 zeroext(128))
define void @call_i32_zext64(i32 %x) {
  call void @take_i32_zext64(i32 %x, i32 %x)
  ret void
}
