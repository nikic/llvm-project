; RUN: opt -passes=globalopt -S < %s | FileCheck %s

; The static constructor can be evaluated through an indirectswitch on a known
; edgeaddress.

; CHECK: @G = local_unnamed_addr global i32 2
@G = global i32 0
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @ctor, ptr null }]

define internal void @ctor() {
entry:
  %p = select i1 true, ptr edgeaddress(@ctor, 1), ptr null
  indirectswitch ptr %p, [ ptr edgeaddress(@ctor, 0), label %a
                           ptr edgeaddress(@ctor, 1), label %b ]
a:
  store i32 1, ptr @G
  ret void
b:
  store i32 2, ptr @G
  ret void
}
