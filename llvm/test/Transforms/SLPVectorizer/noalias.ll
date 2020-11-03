; RUN: opt -S < %s -slp-vectorizer -slp-max-reg-size=128 -slp-min-reg-size=128 | FileCheck %s

; SLP vectorization across a @llvm.provenance.noalias and provenance

; Function Attrs: inaccessiblememonly nounwind willreturn
declare void @llvm.sideeffect() #0

define void @test(float* %p) {
; CHECK-LABEL: @test(
; CHECK-NEXT:    [[P1_DECL:%.*]] = tail call i8* @llvm.noalias.decl.p0i8.p0p0f32.i32(float** null, i32 0, metadata !0)
; CHECK-NEXT:    [[P0:%.*]] = getelementptr float, float* [[P:%.*]], i64 0
; CHECK-NEXT:    [[P1:%.*]] = getelementptr float, float* [[P]], i64 1
; CHECK-NEXT:    [[PROVENANCE_P1:%.*]] = tail call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* [[P1]], i8* [[P1_DECL]], float** null, float** undef, i32 0, metadata !0), !noalias !0
; CHECK-NEXT:    [[P2:%.*]] = getelementptr float, float* [[P]], i64 2
; CHECK-NEXT:    [[P3:%.*]] = getelementptr float, float* [[P]], i64 3
; CHECK-NEXT:    call void @llvm.sideeffect()
; CHECK-NEXT:    [[TMP1:%.*]] = bitcast float* [[P0]] to <4 x float>*
; CHECK-NEXT:    [[TMP2:%.*]] = load <4 x float>, <4 x float>* [[TMP1]], align 4
; CHECK-NEXT:    call void @llvm.sideeffect()
; CHECK-NEXT:    [[TMP3:%.*]] = bitcast float* [[P0]] to <4 x float>*
; CHECK-NEXT:    store <4 x float> [[TMP2]], <4 x float>* [[TMP3]], align 4
; CHECK-NEXT:    ret void
;
  %p1.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0f32.i32(float** null, i32 0, metadata !0)
  %p0 = getelementptr float, float* %p, i64 0
  %p1 = getelementptr float, float* %p, i64 1
  %prov.p1 = tail call float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float* %p1, i8* %p1.decl, float** null, float** undef, i32 0, metadata !0), !noalias !0
  %p2 = getelementptr float, float* %p, i64 2
  %p3 = getelementptr float, float* %p, i64 3
  %l0 = load float, float* %p0, !noalias !0
  %l1 = load float, float* %p1, ptr_provenance float* %prov.p1, !noalias !0
  %l2 = load float, float* %p2, !noalias !0
  call void @llvm.sideeffect()
  %l3 = load float, float* %p3, !noalias !0
  store float %l0, float* %p0, !noalias !0
  call void @llvm.sideeffect()
  store float %l1, float* %p1, ptr_provenance float* %prov.p1, !noalias !0
  store float %l2, float* %p2, !noalias !0
  store float %l3, float* %p3, !noalias !0
  ret void
}

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0f32.i32(float**, i32, metadata) #1

; Function Attrs: nounwind readnone speculatable
declare float* @llvm.provenance.noalias.p0f32.p0i8.p0p0f32.p0p0f32.i32(float*, i8*, float**, float**, i32, metadata) #2

attributes #0 = { inaccessiblememonly nounwind willreturn }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind readnone speculatable }

!0 = !{!1}
!1 = distinct !{!1, !2, !"test_f: p"}
!2 = distinct !{!2, !"test_f"}
