; RUN: opt -S -passes=mergefunc < %s | FileCheck %s
; RUN: opt -S -passes=mergefunc -mergefunc-use-aliases < %s | FileCheck %s

; @f and @g are identical, but the addresses of their blocks are used in global
; tables. Merging them would discard the body of one of them, turning its
; block addresses in the table into dangling values (inttoptr 1), so they must
; not be merged.

; CHECK: @tf = constant [2 x ptr] [ptr blockaddress(@f, %a), ptr blockaddress(@f, %b)]
; CHECK: @tg = constant [2 x ptr] [ptr blockaddress(@g, %a), ptr blockaddress(@g, %b)]
@tf = constant [2 x ptr] [ptr blockaddress(@f, %a), ptr blockaddress(@f, %b)]
@tg = constant [2 x ptr] [ptr blockaddress(@g, %a), ptr blockaddress(@g, %b)]

; CHECK-LABEL: define i32 @f(
; CHECK: indirectbr ptr %t, [label %a, label %b]
define i32 @f(ptr %t) unnamed_addr {
entry:
  indirectbr ptr %t, [label %a, label %b]
a:
  ret i32 1
b:
  ret i32 2
}

; CHECK-LABEL: define i32 @g(
; CHECK: indirectbr ptr %t, [label %a, label %b]
define i32 @g(ptr %t) unnamed_addr {
entry:
  indirectbr ptr %t, [label %a, label %b]
a:
  ret i32 1
b:
  ret i32 2
}
