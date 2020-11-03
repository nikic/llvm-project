; RUN: opt -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=0 -S < %s | FileCheck %s -check-prefix=MD-SCOPE
; RUN: opt -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=1 -S < %s | FileCheck %s -check-prefix=INTR-SCOPE
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @hello(float* noalias nocapture %a, float* nocapture readonly %c) #0 {
entry:
  %0 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 5
  store float %0, float* %arrayidx, align 4
  ret void
}

define void @foo(float* nocapture %a, float* nocapture readonly %c) #0 {
entry:
  tail call void @hello(float* %a, float* %c)
  %0 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 7
  store float %0, float* %arrayidx, align 4
  ret void
}

; MD-SCOPE: define void @foo(float* nocapture %a, float* nocapture readonly %c) #0 {
; MD-SCOPE: entry:
; MD-SCOPE:   %0 = load float, float* %c, align 4, !noalias !0
; MD-SCOPE:   %arrayidx.i = getelementptr inbounds float, float* %a, i64 5
; MD-SCOPE:   store float %0, float* %arrayidx.i, align 4, !alias.scope !0
; MD-SCOPE:   %1 = load float, float* %c, align 4
; MD-SCOPE:   %arrayidx = getelementptr inbounds float, float* %a, i64 7
; MD-SCOPE:   store float %1, float* %arrayidx, align 4
; MD-SCOPE:   ret void
; MD-SCOPE: }

; INTR-SCOPE: define void @foo(float* nocapture %a, float* nocapture readonly %c) #0 {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0f32.i64(float** null, i64 0, metadata !0)
; INTR-SCOPE:   %1 = call float* @llvm.noalias.p0f32.p0i8.p0p0f32.i64(float* %a, i8* %0, float** null, i64 0, metadata !0)
; INTR-SCOPE:   %2 = load float, float* %c, align 4, !noalias !0
; INTR-SCOPE:   %arrayidx.i = getelementptr inbounds float, float* %1, i64 5
; INTR-SCOPE:   store float %2, float* %arrayidx.i, align 4, !noalias !0
; INTR-SCOPE:   %3 = load float, float* %c, align 4
; INTR-SCOPE:   %arrayidx = getelementptr inbounds float, float* %a, i64 7
; INTR-SCOPE:   store float %3, float* %arrayidx, align 4
; INTR-SCOPE:   ret void
; INTR-SCOPE: }

define void @hello2(float* noalias nocapture %a, float* noalias nocapture %b, float* nocapture readonly %c) #0 {
entry:
  %0 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 5
  store float %0, float* %arrayidx, align 4
  %arrayidx1 = getelementptr inbounds float, float* %b, i64 8
  store float %0, float* %arrayidx1, align 4
  ret void
}

define void @foo2(float* nocapture %a, float* nocapture %b, float* nocapture readonly %c) #0 {
entry:
  tail call void @hello2(float* %a, float* %b, float* %c)
  %0 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 7
  store float %0, float* %arrayidx, align 4
  ret void
}

; MD-SCOPE: define void @foo2(float* nocapture %a, float* nocapture %b, float* nocapture readonly %c) #0 {
; MD-SCOPE: entry:
; MD-SCOPE:   %0 = load float, float* %c, align 4, !noalias !3
; MD-SCOPE:   %arrayidx.i = getelementptr inbounds float, float* %a, i64 5
; MD-SCOPE:   store float %0, float* %arrayidx.i, align 4, !alias.scope !7, !noalias !8
; MD-SCOPE:   %arrayidx1.i = getelementptr inbounds float, float* %b, i64 8
; MD-SCOPE:   store float %0, float* %arrayidx1.i, align 4, !alias.scope !8, !noalias !7
; MD-SCOPE:   %1 = load float, float* %c, align 4
; MD-SCOPE:   %arrayidx = getelementptr inbounds float, float* %a, i64 7
; MD-SCOPE:   store float %1, float* %arrayidx, align 4
; MD-SCOPE:   ret void
; MD-SCOPE: }

; INTR-SCOPE: define void @foo2(float* nocapture %a, float* nocapture %b, float* nocapture readonly %c) #0 {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0f32.i64(float** null, i64 0, metadata !3)
; INTR-SCOPE:   %1 = call float* @llvm.noalias.p0f32.p0i8.p0p0f32.i64(float* %a, i8* %0, float** null, i64 0, metadata !3)
; INTR-SCOPE:   %2 = call i8* @llvm.noalias.decl.p0i8.p0p0f32.i64(float** null, i64 0, metadata !6)
; INTR-SCOPE:   %3 = call float* @llvm.noalias.p0f32.p0i8.p0p0f32.i64(float* %b, i8* %2, float** null, i64 0, metadata !6)
; INTR-SCOPE:   %4 = load float, float* %c, align 4, !noalias !8
; INTR-SCOPE:   %arrayidx.i = getelementptr inbounds float, float* %1, i64 5
; INTR-SCOPE:   store float %4, float* %arrayidx.i, align 4, !noalias !8
; INTR-SCOPE:   %arrayidx1.i = getelementptr inbounds float, float* %3, i64 8
; INTR-SCOPE:   store float %4, float* %arrayidx1.i, align 4, !noalias !8
; INTR-SCOPE:   %5 = load float, float* %c, align 4
; INTR-SCOPE:   %arrayidx = getelementptr inbounds float, float* %a, i64 7
; INTR-SCOPE:   store float %5, float* %arrayidx, align 4
; INTR-SCOPE:   ret void
; INTR-SCOPE: }

attributes #0 = { nounwind uwtable }
attributes #1 = { argmemonly nounwind }

; MD-SCOPE: !0 = !{!1}
; MD-SCOPE: !1 = distinct !{!1, !2, !"hello: %a"}
; MD-SCOPE: !2 = distinct !{!2, !"hello"}
; MD-SCOPE: !3 = !{!4, !6}
; MD-SCOPE: !4 = distinct !{!4, !5, !"hello2: %a"}
; MD-SCOPE: !5 = distinct !{!5, !"hello2"}
; MD-SCOPE: !6 = distinct !{!6, !5, !"hello2: %b"}
; MD-SCOPE: !7 = !{!4}
; MD-SCOPE: !8 = !{!6}

; INTR-SCOPE: !0 = !{!1}
; INTR-SCOPE: !1 = distinct !{!1, !2, !"hello: %a"}
; INTR-SCOPE: !2 = distinct !{!2, !"hello"}
; INTR-SCOPE: !3 = !{!4}
; INTR-SCOPE: !4 = distinct !{!4, !5, !"hello2: %a"}
; INTR-SCOPE: !5 = distinct !{!5, !"hello2"}
; INTR-SCOPE: !6 = !{!7}
; INTR-SCOPE: !7 = distinct !{!7, !5, !"hello2: %b"}
; INTR-SCOPE: !8 = !{!4, !7}
