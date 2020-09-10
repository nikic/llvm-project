; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
; RUN: opt < %s -aa-pipeline=basic-aa,scoped-noalias-aa -passes=aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind
define dso_local void @test_prp_prp(i32** nocapture readonly %_pA, i32** nocapture readonly %_pB) local_unnamed_addr #0 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i64(i32*** null, i64 0, metadata !2)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i64(i32*** null, i64 0, metadata !5)
  %2 = tail call i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i64(i32** %_pA, i8* %0, i32*** null, i32*** undef, i64 0, metadata !2), !tbaa !7, !noalias !11
  %3 = load i32*, i32** %_pA, ptr_provenance i32** %2, align 4, !tbaa !7, !noalias !11
  store i32 42, i32* %3, ptr_provenance i32* undef, align 4, !tbaa !12, !noalias !11
  %4 = tail call i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i64(i32** %_pB, i8* %1, i32*** null, i32*** undef, i64 0, metadata !5), !tbaa !7, !noalias !11
  %5 = load i32*, i32** %_pB, ptr_provenance i32** %4, align 4, !tbaa !7, !noalias !11
  store i32 99, i32* %5, ptr_provenance i32* undef, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test_prp_prp:
; CHECK:  MayAlias:   store i32 99, i32* %5, ptr_provenance i32* undef, align 4, !tbaa !12, !noalias !11 <->   store i32 42, i32* %3, ptr_provenance i32* undef, align 4, !tbaa !12, !noalias !11

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i64(i32***, i64, metadata) #1

; Function Attrs: nofree nounwind
define dso_local void @test_rpp_rpp(i32** nocapture %_pA, i32** nocapture %_pB) local_unnamed_addr #2 !noalias !14 {
entry:
  %0 = load i32*, i32** %_pA, ptr_provenance i32** undef, align 4, !tbaa !7, !noalias !14
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %0, i8* null, i32** %_pA, i32** undef, i64 0, metadata !14), !tbaa !7, !noalias !14
  store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !12, !noalias !14
  %2 = load i32*, i32** %_pB, ptr_provenance i32** undef, align 4, !tbaa !7, !noalias !14
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* null, i32** %_pB, i32** undef, i64 0, metadata !14), !tbaa !7, !noalias !14
  store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !12, !noalias !14
  ret void
}
; CHECK-LABEL: Function: test_rpp_rpp:
; CHECK:  MayAlias:   store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !2 <->   store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2

; Function Attrs: nounwind
define dso_local void @test_rprp_rprp(i32** nocapture %_pA, i32** nocapture %_pB) local_unnamed_addr #0 !noalias !17 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i64(i32*** null, i64 0, metadata !20)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i64(i32*** null, i64 0, metadata !22)
  %2 = tail call i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i64(i32** %_pA, i8* %0, i32*** null, i32*** undef, i64 0, metadata !20), !tbaa !7, !noalias !24
  %3 = load i32*, i32** %_pA, ptr_provenance i32** %2, align 4, !tbaa !7, !noalias !24
  %4 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %3, i8* null, i32** %_pA, i32** %2, i64 0, metadata !17), !tbaa !7, !noalias !24
  store i32 42, i32* %3, ptr_provenance i32* %4, align 4, !tbaa !12, !noalias !24
  %5 = tail call i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i64(i32** %_pB, i8* %1, i32*** null, i32*** undef, i64 0, metadata !22), !tbaa !7, !noalias !24
  %6 = load i32*, i32** %_pB, ptr_provenance i32** %5, align 4, !tbaa !7, !noalias !24
  %7 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %6, i8* null, i32** %_pB, i32** %5, i64 0, metadata !17), !tbaa !7, !noalias !24
  store i32 99, i32* %6, ptr_provenance i32* %7, align 4, !tbaa !12, !noalias !24
  ret void
}

; CHECK-LABEL: Function: test_rprp_rprp:
; CHECK:  NoAlias:   store i32 99, i32* %6, ptr_provenance i32* %7, align 4, !tbaa !14, !noalias !13 <->   store i32 42, i32* %3, ptr_provenance i32* %4, align 4, !tbaa !14, !noalias !13

; Function Attrs: nounwind
define dso_local void @test_prp_01(i32** nocapture %pA) local_unnamed_addr #0 !noalias !17 {
entry:
  %0 = load i32*, i32** %pA, ptr_provenance i32** undef, align 8, !tbaa !7, !noalias !17
  %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %0, i8* null, i32** %pA, i32** undef, i64 0, metadata !17), !tbaa !7, !noalias !17
  store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !12, !noalias !17
  %arrayidx1 = getelementptr inbounds i32*, i32** %pA, i64 0
  %2 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 8, !tbaa !7, !noalias !17
  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* null, i32** %arrayidx1, i32** undef, i64 0, metadata !17), !tbaa !7, !noalias !17
  store i32 43, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !12, !noalias !17
  ret void
}

; CHECK-LABEL: Function: test_prp_01:
; CHECK:  MayAlias: store i32 43, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !2 <-> store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2
; Function Attrs: nounwind
define dso_local void @test_prp_02(i32** nocapture %pA) local_unnamed_addr #0 !noalias !17 {
entry:
  %0 = load i32*, i32** %pA, ptr_provenance i32** undef, align 8, !tbaa !7, !noalias !17
  %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %0, i8* null, i32** %pA, i32** undef, i64 0, metadata !17), !tbaa !7, !noalias !17
  store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !12, !noalias !17
  %arrayidx1 = getelementptr inbounds i32*, i32** %pA, i64 1
  %2 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 8, !tbaa !7, !noalias !17
  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* null, i32** %arrayidx1, i32** undef, i64 0, metadata !17), !tbaa !7, !noalias !17
  store i32 43, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !12, !noalias !17
  ret void
}

; CHECK-LABEL: Function: test_prp_02:
; CHECK:  NoAlias: store i32 43, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !2 <-> store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2

; Function Attrs: nounwind readnone speculatable
declare i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i64(i32**, i8*, i32***, i32***, i64, metadata) #3

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32*, i8*, i32**, i32**, i64, metadata) #3

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_prp_prp: pA"}
!4 = distinct !{!4, !"test_prp_prp"}
!5 = !{!6}
!6 = distinct !{!6, !4, !"test_prp_prp: pB"}
!7 = !{!8, !8, i64 0, i64 4}
!8 = !{!9, i64 4, !"any pointer"}
!9 = !{!10, i64 1, !"omnipotent char"}
!10 = !{!"Simple C/C++ TBAA"}
!11 = !{!3, !6}
!12 = !{!13, !13, i64 0, i64 4}
!13 = !{!9, i64 4, !"int"}
!14 = !{!15}
!15 = distinct !{!15, !16, !"test_rpp_rpp: unknown scope"}
!16 = distinct !{!16, !"test_rpp_rpp"}
!17 = !{!18}
!18 = distinct !{!18, !19, !"test_rprp_rprp: unknown scope"}
!19 = distinct !{!19, !"test_rprp_rprp"}
!20 = !{!21}
!21 = distinct !{!21, !19, !"test_rprp_rprp: pA"}
!22 = !{!23}
!23 = distinct !{!23, !19, !"test_rprp_rprp: pB"}
!24 = !{!21, !23, !18}
