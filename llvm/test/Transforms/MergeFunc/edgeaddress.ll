; RUN: opt -passes=mergefunc -S < %s | FileCheck %s --check-prefixes=CHECK,THUNK
; RUN: opt -passes=mergefunc -mergefunc-use-aliases -S < %s | FileCheck %s --check-prefixes=CHECK,ALIAS

; When merging @g into the equivalent @f, the edge addresses of @g (which may
; be passed to @g) must be transferred to @f.

; CHECK: @tf = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]
; CHECK: @tg = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]
; CHECK: @th = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]
@tf = constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]
@tg = constant [2 x ptr] [ptr edgeaddress(@g, 0), ptr edgeaddress(@g, 1)]
@th = constant [2 x ptr] [ptr edgeaddress(@h, 0), ptr edgeaddress(@h, 1)]

; Both @wf and @wg are interposable, so both become thunks to a new private
; function with the body of @wf, which keeps the edge addresses of @wf.
; CHECK: @twf = constant [1 x ptr] [ptr edgeaddress(@[[WF:[0-9]+]], 0)]
; CHECK: @twg = constant [1 x ptr] [ptr edgeaddress(@[[WF]], 0)]
@twf = constant [1 x ptr] [ptr edgeaddress(@wf, 0)]
@twg = constant [1 x ptr] [ptr edgeaddress(@wg, 0)]

; ALIAS: @g = unnamed_addr alias i32 (ptr), ptr @f

; CHECK-NOT: define {{.*}} @h(

; CHECK-LABEL: define i32 @f(
; CHECK: indirectswitch ptr %t, [
; CHECK-NEXT: ptr edgeaddress(@f, 0), label %a
; CHECK-NEXT: ptr edgeaddress(@f, 1), label %b
define i32 @f(ptr %t) unnamed_addr {
entry:
  indirectswitch ptr %t, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 1), label %b ]
a:
  ret i32 1
b:
  ret i32 2
}

define i32 @g(ptr %t) unnamed_addr {
entry:
  indirectswitch ptr %t, [ ptr edgeaddress(@g, 0), label %a
                           ptr edgeaddress(@g, 1), label %b ]
a:
  ret i32 1
b:
  ret i32 2
}

; @h is internal and without uses, so it is erased.
define internal i32 @h(ptr %t) unnamed_addr {
entry:
  indirectswitch ptr %t, [ ptr edgeaddress(@h, 0), label %a
                           ptr edgeaddress(@h, 1), label %b ]
a:
  ret i32 1
b:
  ret i32 2
}

; CHECK: define private i32 @[[WF]](
; CHECK: indirectswitch ptr %t, [
; CHECK-NEXT: ptr edgeaddress(@[[WF]], 0), label %a
define weak i32 @wf(ptr %t) {
entry:
  call void @dummy()
  indirectswitch ptr %t, [ ptr edgeaddress(@wf, 0), label %a ]
a:
  ret i32 3
}

define weak i32 @wg(ptr %t) {
entry:
  call void @dummy()
  indirectswitch ptr %t, [ ptr edgeaddress(@wg, 0), label %a ]
a:
  ret i32 3
}

; THUNK-LABEL: define i32 @g(
; THUNK-NEXT: tail call i32 @f(

declare void @dummy()
