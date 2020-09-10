; RUN: opt -basic-aa -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=0 -S < %s | FileCheck %s -check-prefix=MD-SCOPE
; RUN: opt -basic-aa -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=1 -S < %s | FileCheck %s -check-prefix=INTR-SCOPE
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture readonly, i64, i1) #0
declare void @hey() #1

define void @hello(i8* noalias nocapture %a, i8* noalias nocapture readonly %c, i8* nocapture %b) #1 {
entry:
  %l = alloca i8, i32 512, align 1
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %b, i64 16, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %c, i64 16, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %c, i64 16, i1 0)
  call void @hey()
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %l, i8* align 16 %c, i64 16, i1 0)
  ret void
}

define void @foo(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) #1 {
entry:
  tail call void @hello(i8* %a, i8* %c, i8* %b)
  ret void
}

; MD-SCOPE: define void @foo(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) #1 {
; MD-SCOPE: entry:
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %b, i64 16, i1 false) #1, !noalias !0
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %c, i64 16, i1 false) #1, !noalias !3
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %c, i64 16, i1 false) #1, !alias.scope !5
; MD-SCOPE:   call void @hey() #1, !noalias !5
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %{{.*}}, i8* align 16 %c, i64 16, i1 false) #1, !noalias !3
; MD-SCOPE:   ret void
; MD-SCOPE: }

; INTR-SCOPE: define void @foo(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) #1 {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %l.i = alloca i8, i32 512, align 1
; INTR-SCOPE:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i8.i64(i8** null, i64 0, metadata !0)
; INTR-SCOPE:   %1 = call i8* @llvm.noalias.p0i8.p0i8.p0p0i8.i64(i8* %a, i8* %0, i8** null, i64 0, metadata !0)
; INTR-SCOPE:   %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i8.i64(i8** null, i64 0, metadata !3)
; INTR-SCOPE:   %3 = call i8* @llvm.noalias.p0i8.p0i8.p0p0i8.i64(i8* %c, i8* %2, i8** null, i64 0, metadata !3)
; INTR-SCOPE:   call void @llvm.lifetime.start.p0i8(i64 512, i8* %l.i)
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %1, i8* align 16 %b, i64 16, i1 false) #1, !noalias !5
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %3, i64 16, i1 false) #1, !noalias !5
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %1, i8* align 16 %3, i64 16, i1 false) #1, !noalias !5
; INTR-SCOPE:   call void @hey() #1, !noalias !5
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %l.i, i8* align 16 %3, i64 16, i1 false) #1, !noalias !5
; INTR-SCOPE:   call void @llvm.lifetime.end.p0i8(i64 512, i8* %l.i)
; INTR-SCOPE:   ret void
; INTR-SCOPE: }

define void @hello_cs(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) #1 {
entry:
  %l = alloca i8, i32 512, align 1
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %b, i64 16, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %c, i64 16, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %c, i64 16, i1 0)
  call void @hey()
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %l, i8* align 16 %c, i64 16, i1 0)
  ret void
}

define void @foo_cs(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) #2 {
entry:
  tail call void @hello_cs(i8* noalias %a, i8* noalias %c, i8* %b)
  ret void
}

; MD-SCOPE: define void @foo_cs(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) {
; MD-SCOPE: entry:
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %b, i64 16, i1 false) #1, !noalias !6
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %c, i64 16, i1 false) #1, !noalias !9
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %c, i64 16, i1 false) #1, !alias.scope !11
; MD-SCOPE:   call void @hey() #1, !noalias !11
; MD-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %l.i, i8* align 16 %c, i64 16, i1 false) #1, !noalias !9
; MD-SCOPE:   ret void
; MD-SCOPE: }

; INTR-SCOPE: define void @foo_cs(i8* nocapture %a, i8* nocapture readonly %c, i8* nocapture %b) {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %l.i = alloca i8, i32 512, align 1
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %b, i64 16, i1 false) #1
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %b, i8* align 16 %c, i64 16, i1 false) #1
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %a, i8* align 16 %c, i64 16, i1 false) #1
; INTR-SCOPE:   call void @hey() #1
; INTR-SCOPE:   call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %l.i, i8* align 16 %c, i64 16, i1 false) #1
; INTR-SCOPE:   ret void
; INTR-SCOPE: }

attributes #0 = { nounwind argmemonly willreturn }
attributes #1 = { nounwind }

; MD-SCOPE:!0 = !{!1}
; MD-SCOPE:!1 = distinct !{!1, !2, !"hello: %c"}
; MD-SCOPE:!2 = distinct !{!2, !"hello"}
; MD-SCOPE:!3 = !{!4}
; MD-SCOPE:!4 = distinct !{!4, !2, !"hello: %a"}
; MD-SCOPE:!5 = !{!4, !1}
; MD-SCOPE:!6 = !{!7}
; MD-SCOPE:!7 = distinct !{!7, !8, !"hello_cs: %c"}
; MD-SCOPE:!8 = distinct !{!8, !"hello_cs"}
; MD-SCOPE:!9 = !{!10}
; MD-SCOPE:!10 = distinct !{!10, !8, !"hello_cs: %a"}
; MD-SCOPE:!11 = !{!10, !7}

; INTR-SCOPE: !0 = !{!1}
; INTR-SCOPE: !1 = distinct !{!1, !2, !"hello: %a"}
; INTR-SCOPE: !2 = distinct !{!2, !"hello"}
; INTR-SCOPE: !3 = !{!4}
; INTR-SCOPE: !4 = distinct !{!4, !2, !"hello: %c"}
; INTR-SCOPE: !5 = !{!1, !4}
