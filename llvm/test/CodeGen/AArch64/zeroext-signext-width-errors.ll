; RUN: not llc -mtriple=aarch64-linux-gnu < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=arm64-apple-darwin < %s 2>&1 | FileCheck %s --check-prefixes=CHECK,DARWIN

; An extension width larger than the location of the argument is ill-formed.

; CHECK: error: argument extension width 64 exceeds the size of the argument location (32 bits) in function 'call_i32_zext64'
declare void @take_i32_zext64(i32 zeroext(64))
define void @call_i32_zext64(i32 %x) {
  call void @take_i32_zext64(i32 %x)
  ret void
}

; CHECK: error: argument extension width 64 exceeds the size of the argument location (32 bits) in function 'arg_i8_sext64'
define i32 @arg_i8_sext64(i8 signext(64) %x) {
  %ext = sext i8 %x to i32
  ret i32 %ext
}

; On Darwin, small integer arguments are packed on the stack.
; DARWIN: error: argument extension width 16 exceeds the size of the argument stack slot (8 bits) in function 'call_stack'
declare void @take_stack(i64, i64, i64, i64, i64, i64, i64, i64, i8 zeroext(16))
define void @call_stack(i8 %x) {
  call void @take_stack(i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i8 %x)
  ret void
}

; DARWIN: error: argument extension width 16 exceeds the size of the argument stack slot (8 bits) in function 'arg_stack'
define i8 @arg_stack(i64, i64, i64, i64, i64, i64, i64, i64, i8 zeroext(16) %x) {
  ret i8 %x
}
