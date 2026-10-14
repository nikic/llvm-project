; RUN: llvm-as < %s | llvm-dis | FileCheck %s
; RUN: opt -S < %s | FileCheck %s

; Test zeroext and signext attributes with and without an explicit extension
; width.

; CHECK: define zeroext i1 @no_width(i8 zeroext %a, i16 signext %b)
define zeroext i1 @no_width(i8 zeroext %a, i16 signext %b) {
  ret i1 false
}

; CHECK: define zeroext(8) i1 @zeroext_width(i1 zeroext(32) %a, i8 zeroext(8) %b, i32 zeroext(64) %c)
define zeroext(8) i1 @zeroext_width(i1 zeroext(32) %a, i8 zeroext(8) %b, i32 zeroext(64) %c) {
  ret i1 %a
}

; CHECK: define signext(32) i8 @signext_width(i1 signext(8) %a, i8 signext(16) %b, i32 signext(64) %c)
define signext(32) i8 @signext_width(i1 signext(8) %a, i8 signext(16) %b, i32 signext(64) %c) {
  ret i8 %b
}

; CHECK: declare zeroext(32) i16 @decl(i1 zeroext(8), i8 signext(32))
declare zeroext(32) i16 @decl(i1 zeroext(8), i8 signext(32))

define void @call_site(i1 %a, i8 %b) {
; CHECK: call zeroext(32) i16 @decl(i1 zeroext(8) %a, i8 signext(32) %b)
  call zeroext(32) i16 @decl(i1 zeroext(8) %a, i8 signext(32) %b)
; CHECK: call zeroext i16 @decl(i1 zeroext %a, i8 signext %b)
  call zeroext i16 @decl(i1 zeroext %a, i8 signext %b)
  ret void
}
