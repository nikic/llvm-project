; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nofree norecurse nounwind uwtable writeonly
define dso_local void @test_p_p(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #0 {
entry:
  store i32 42, i32* %_pA, align 4, !tbaa !2
  store i32 99, i32* %_pB, align 4, !tbaa !2
  ret void
}
; CHECK-LABEL: Function: test_p_p:
; CHECK:   MayAlias:   store i32 99, i32* %_pB, align 4, !tbaa !2 <->   store i32 42, i32* %_pA, align 4, !tbaa !2

; Function Attrs: nounwind uwtable
define dso_local void @test_rp_p(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !6)
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !6), !tbaa !9, !noalias !6
  store i32 42, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !2, !noalias !6
  store i32 99, i32* %_pB, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !6
  ret void
}
; CHECK-LABEL: Function: test_rp_p:
; CHECK:  NoAlias:   store i32 99, i32* %_pB, ptr_provenance i32* undef, align 4, !tbaa !9, !noalias !2 <->   store i32 42, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #2

; Function Attrs: nounwind uwtable
define dso_local void @test_rp_rp_00(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !14)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !16
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* %1, i32** null, i32** undef, i32 0, metadata !14), !tbaa !9, !noalias !16
  store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !16
  ret void
}
; CHECK-LABEL: Function: test_rp_rp_00:
; CHECK:  NoAlias:   store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !12, !noalias !11 <->   store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !12, !noalias !11

; Now test variants: {objectP, objectID, Scope }
; NOTE: in the following tests, the Scope information is recycled from previous tests

; Same info -> MayAlias
; Function Attrs: nounwind uwtable
define dso_local void @test_rp_rp_01(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %a.p = alloca i32*, align 8
  %b.p = alloca i32*, align 8
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %a.p, i32 0, metadata !11)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %b.p, i32 0, metadata !11)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* null, i32** %a.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !16
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* null, i32** %a.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !16
  ret void
}
; CHECK-LABEL: Function: test_rp_rp_01:
; CHECK:  MayAlias:   store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !11, !noalias !9 <->   store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !11, !noalias !9

; Variants with different info -> NoAlias

; Function Attrs: nounwind uwtable
define dso_local void @test_rp_rp_02(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %a.p = alloca i32*, align 8
  %b.p = alloca i32*, align 8
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %a.p, i32 0, metadata !11)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %b.p, i32 0, metadata !11)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* null, i32** %a.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !16
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* null, i32** %b.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !16
  ret void
}
; CHECK-LABEL: Function: test_rp_rp_02:
; CHECK:    NoAlias:   store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !11, !noalias !9 <->   store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !11, !noalias !9

; Function Attrs: nounwind uwtable
define dso_local void @test_rp_rp_03(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %a.p = alloca i32*, align 8
  %b.p = alloca i32*, align 8
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %a.p, i32 0, metadata !11)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %b.p, i32 0, metadata !11)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* null, i32** %a.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !16
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* null, i32** %a.p, i32** undef, i32 1, metadata !11), !tbaa !9, !noalias !16
  store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !16
  ret void
}
; CHECK-LABEL: Function: test_rp_rp_03:
; CHECK:    NoAlias:   store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !11, !noalias !9 <->   store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !11, !noalias !9

; Function Attrs: nounwind uwtable
define dso_local void @test_rp_rp_04(i32* nocapture %_pA, i32* nocapture %_pB) local_unnamed_addr #1 {
entry:
  %a.p = alloca i32*, align 8
  %b.p = alloca i32*, align 8
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %a.p, i32 0, metadata !11)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** %b.p, i32 0, metadata !11)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* null, i32** %a.p, i32** undef, i32 0, metadata !11), !tbaa !9, !noalias !16
  store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !16
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* null, i32** %a.p, i32** undef, i32 0, metadata !14), !tbaa !9, !noalias !16
  store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !16
  ret void
}
; CHECK-LABEL: Function: test_rp_rp_04:
; CHECK:    NoAlias:   store i32 99, i32* %_pB, ptr_provenance i32* %3, align 4, !tbaa !11, !noalias !9 <->   store i32 42, i32* %_pA, ptr_provenance i32* %2, align 4, !tbaa !11, !noalias !9


; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #3

attributes #0 = { nofree norecurse nounwind uwtable writeonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7}
!7 = distinct !{!7, !8, !"test_rp_p: pA"}
!8 = distinct !{!8, !"test_rp_p"}
!9 = !{!10, !10, i64 0}
!10 = !{!"any pointer", !4, i64 0}
!11 = !{!12}
!12 = distinct !{!12, !13, !"test_rp_rp: pA"}
!13 = distinct !{!13, !"test_rp_rp"}
!14 = !{!15}
!15 = distinct !{!15, !13, !"test_rp_rp: pB"}
!16 = !{!12, !15}
