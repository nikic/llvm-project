; RUN: not llvm-as -disable-output %s 2>&1 | FileCheck %s

; CHECK: Extension width of zeroext/signext attribute must not be smaller than the bit width of the type!
; CHECK-NEXT: ptr @arg_too_small
define void @arg_too_small(i16 zeroext(8) %a) {
  ret void
}

; CHECK: Extension width of zeroext/signext attribute must not be smaller than the bit width of the type!
; CHECK-NEXT: ptr @ret_too_small
define signext(16) i32 @ret_too_small() {
  ret i32 0
}

; CHECK: Extension width of zeroext/signext attribute must not be smaller than the bit width of the type!
; CHECK-NEXT: call void @arg_too_small(i16 signext(15) 0)
define void @call_too_small() {
  call void @arg_too_small(i16 signext(15) 0)
  ret void
}

; CHECK: Attribute 'zeroext(8)' applied to incompatible type!
; CHECK-NEXT: ptr @not_int
define void @not_int(ptr zeroext(8) %p) {
  ret void
}

; Same width as the type is allowed.
; CHECK-NOT: @same_width
define zeroext(8) i8 @same_width(i16 signext(16) %a) {
  ret i8 0
}
