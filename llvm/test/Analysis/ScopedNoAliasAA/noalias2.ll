; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define void @foo(float* noalias nocapture %a, float* noalias nocapture readonly %c) #0 {
entry:
  %0 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %a, i8* null, float** null, float** null, i32 0, metadata !0) #1
  %1 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %c, i8* null, float** null, float** null, i32 0, metadata !3) #1
  %2 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !5
  %arrayidx.i = getelementptr inbounds float, float* %a, i64 5
  store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
  %3 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5
  %arrayidx = getelementptr inbounds float, float* %a, i64 7
  store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5
  ret void
}

; CHECK-LABEL: Function: foo:
; CHECK: NoAlias:   %2 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: NoAlias:   %2 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !5 <->   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5
; CHECK: NoAlias:   %3 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: NoAlias:   %3 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5 <->   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5
; CHECK: NoAlias:   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5

; Function Attrs: nounwind uwtable
define void @foo2(float* nocapture %a, float* nocapture %b, float* nocapture readonly %c) #0 {
entry:
  %0 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %a, i8* null, float** null, float** null, i32 0, metadata !6) #1
  %1 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %c, i8* null, float** null, float** null, i32 0, metadata !9) #1
  %2 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %0, i8* null, float** null, float** null, i32 0, metadata !11) #1, !noalias !14
  %3 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %1, i8* null, float** null, float** null, i32 0, metadata !15) #1, !noalias !14
  %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !17
  %arrayidx.i.i = getelementptr inbounds float, float* %a, i64 5
  store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !17
  %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !14
  %arrayidx.i = getelementptr inbounds float, float* %a, i64 7
  store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !14
  %6 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %a, i8* null, float** null, float** null, i32 0, metadata !18) #1
  %7 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %b, i8* null, float** null, float** null, i32 0, metadata !21) #1
  %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !23
  %arrayidx.i1 = getelementptr inbounds float, float* %a, i64 6
  store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !23
  %arrayidx1.i = getelementptr inbounds float, float* %b, i64 8
  store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !23
  ; %9 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !23
  %9 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 7
  store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !23
  ret void
}

; CHECK-LABEL: Function: foo2:
; CHECK: NoAlias:   %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !11 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: NoAlias:   %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !11 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: MayAlias:   %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !11 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: MayAlias:   %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !11 <->   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17
; CHECK: MayAlias:   %4 = load float, float* %c, ptr_provenance float* %3, align 4, !noalias !11 <->   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17
; CHECK: NoAlias:   %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !8 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: NoAlias:   %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !8 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: MayAlias:   %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !8 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: MayAlias:   %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !8 <->   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17
; CHECK: MayAlias:   %5 = load float, float* %c, ptr_provenance float* %1, align 4, !noalias !8 <->   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17
; CHECK: MayAlias:   %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !17 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: MayAlias:   %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !17 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: NoAlias:   %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !17 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: NoAlias:   %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !17 <->   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17
; CHECK: MayAlias:   %8 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !17 <->   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17
; CHECK: MayAlias:   %9 = load float, float* %c, align 4 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: MayAlias:   %9 = load float, float* %c, align 4 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: MayAlias:   %9 = load float, float* %c, align 4 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: MayAlias:   %9 = load float, float* %c, align 4 <->   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17
; CHECK: MayAlias:   %9 = load float, float* %c, align 4 <->   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17
; CHECK: NoAlias:   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: NoAlias:   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: NoAlias:   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: MayAlias:   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: MayAlias:   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: NoAlias:   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: NoAlias:   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17 <->   store float %4, float* %arrayidx.i.i, ptr_provenance float* %2, align 4, !noalias !11
; CHECK: MustAlias:   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17 <->   store float %5, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
; CHECK: NoAlias:   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17 <->   store float %8, float* %arrayidx.i1, ptr_provenance float* %6, align 4, !noalias !17
; CHECK: NoAlias:   store float %9, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !17 <->   store float %8, float* %arrayidx1.i, ptr_provenance float* %7, align 4, !noalias !17

declare float*  @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float*, i8*, float**, float**, i32, metadata ) nounwind

attributes #0 = { nounwind uwtable }
attributes #1 = { nounwind }

!0 = !{!1}
!1 = distinct !{!1, !2, !"hello: %a"}
!2 = distinct !{!2, !"hello"}
!3 = !{!4}
!4 = distinct !{!4, !2, !"hello: %c"}
!5 = !{!1, !4}
!6 = !{!7}
!7 = distinct !{!7, !8, !"foo: %a"}
!8 = distinct !{!8, !"foo"}
!9 = !{!10}
!10 = distinct !{!10, !8, !"foo: %c"}
!11 = !{!12}
!12 = distinct !{!12, !13, !"hello: %a"}
!13 = distinct !{!13, !"hello"}
!14 = !{!7, !10}
!15 = !{!16}
!16 = distinct !{!16, !13, !"hello: %c"}
!17 = !{!12, !16, !7, !10}
!18 = !{!19}
!19 = distinct !{!19, !20, !"hello2: %a"}
!20 = distinct !{!20, !"hello2"}
!21 = !{!22}
!22 = distinct !{!22, !20, !"hello2: %b"}
!23 = !{!19, !22}
