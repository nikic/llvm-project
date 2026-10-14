; RUN: not llc -mtriple=x86_64-linux-gnu < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=i686-linux-gnu < %s 2>&1 | FileCheck %s --check-prefixes=CHECK,X86

; An extension width larger than the location of the argument or return value
; is ill-formed.

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

; X86: error: return value extension width 64 exceeds the size of the return register in function 'ret_i32_sext64'
define signext(64) i32 @ret_i32_sext64(i32 %x) {
  ret i32 %x
}

; CHECK: error: return value extension width 128 exceeds the size of the return register
declare zeroext(128) i32 @get_i32_zext128()
define i32 @use_i32_zext128() {
  %r = call zeroext(128) i32 @get_i32_zext128()
  ret i32 %r
}
