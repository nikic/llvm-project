; RUN: opt -passes=attributor -attributor-allow-deep-wrappers -S < %s | FileCheck %s

; A function containing an indirectswitch must not be internalized (copied):
; the edge addresses passed to the copy would still refer to the original.

@tbl = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]

; CHECK-NOT: @f.internalized
; CHECK: define linkonce_odr i32 @f(
; CHECK-NOT: @f.internalized
define linkonce_odr i32 @f(i32 %i) {
entry:
  %p = getelementptr ptr, ptr @tbl, i32 %i
  %a = load ptr, ptr %p
  indirectswitch ptr %a, [ ptr edgeaddress(@f, 0), label %x
                           ptr edgeaddress(@f, 1), label %y ]
x:
  ret i32 10
y:
  ret i32 20
}

define i32 @g(i32 %i) {
  %r = call i32 @f(i32 %i)
  ret i32 %r
}
