; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define void @foo(float* nocapture %a, float* nocapture readonly %c, i64 %i0, i64 %i1) #0 {
entry:
  %prov.a = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %a, i8* null, float** null, float** null, i32 0, metadata !0) #1
  %0 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !0
  %arrayidx.i = getelementptr inbounds float, float* %a, i64 %i0
  store float %0, float* %arrayidx.i, ptr_provenance float* %prov.a, align 4, !noalias !0
  %1 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 %i1
  store float %1, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !0
  ret void
}

; CHECK-LABEL: Function: foo:
; CHECK: NoAlias:   %0 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !0 <->   store float %0, float* %arrayidx.i, ptr_provenance float* %prov.a, align 4, !noalias !0
; CHECK: MayAlias:   %0 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !0 <->   store float %1, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !0
; CHECK: MayAlias:   %1 = load float, float* %c, align 4 <->   store float %0, float* %arrayidx.i, ptr_provenance float* %prov.a, align 4, !noalias !0
; CHECK: MayAlias:   %1 = load float, float* %c, align 4 <->   store float %1, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !0
; CHECK: NoAlias:   store float %1, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !0 <->   store float %0, float* %arrayidx.i, ptr_provenance float* %prov.a, align 4, !noalias !0


; Function Attrs: nounwind uwtable
define void @foo2(float* nocapture %a, float* nocapture %b, float* nocapture readonly %c, i64 %i0, i64 %i1) #0 {
entry:
  %0 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %a, i8* null, float** null, float** null, i32 0, metadata !3) #1
  %1 = call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %b, i8* null, float** null, float** null, i32 0, metadata !6) #1
  %2 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !8
  %arrayidx.i = getelementptr inbounds float, float* %a, i64 5
  store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !8
  %arrayidx1.i = getelementptr inbounds float, float* %b, i64 %i0
  store float %2, float* %arrayidx1.i, ptr_provenance float* %1, align 4, !noalias !8
  %3 = load float, float* %c, align 4
  %arrayidx = getelementptr inbounds float, float* %a, i64 %i1
  store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !8
  ret void
}

; CHECK-LABEL: Function: foo2:
; CHECK: NoAlias:   %2 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: NoAlias:   %2 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx1.i, ptr_provenance float* %1, align 4, !noalias !5
; CHECK: MayAlias:   %2 = load float, float* %c, ptr_provenance float* null, align 4, !noalias !5 <->   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5
; CHECK: MayAlias:   %3 = load float, float* %c, align 4 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: MayAlias:   %3 = load float, float* %c, align 4 <->   store float %2, float* %arrayidx1.i, ptr_provenance float* %1, align 4, !noalias !5
; CHECK: MayAlias:   %3 = load float, float* %c, align 4 <->   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5
; CHECK: NoAlias:   store float %2, float* %arrayidx1.i, ptr_provenance float* %1, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: NoAlias:   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx.i, ptr_provenance float* %0, align 4, !noalias !5
; CHECK: NoAlias:   store float %3, float* %arrayidx, ptr_provenance float* null, align 4, !noalias !5 <->   store float %2, float* %arrayidx1.i, ptr_provenance float* %1, align 4, !noalias !5

declare float*  @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float*, i8*, float**, float**, i32, metadata ) nounwind

attributes #0 = { nounwind uwtable }
attributes #1 = { nounwind }

!0 = !{!1}
!1 = distinct !{!1, !2, !"hello: %a"}
!2 = distinct !{!2, !"hello"}
!3 = !{!4}
!4 = distinct !{!4, !5, !"hello2: %a"}
!5 = distinct !{!5, !"hello2"}
!6 = !{!7}
!7 = distinct !{!7, !5, !"hello2: %b"}
!8 = !{!4, !7}
