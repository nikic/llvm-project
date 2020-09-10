; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.FOO = type { i32*, i32* }

; Function Attrs: nofree nounwind
define dso_local void @test_prp0_prp1(i32** nocapture %_pA) local_unnamed_addr #0 !noalias !2 {
entry:
  %0 = load i32*, i32** %_pA, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %0, i8* null, i32** %_pA, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  %arrayidx1 = getelementptr inbounds i32*, i32** %_pA, i32 1
  %2 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %2, i8* null, i32** nonnull %arrayidx1, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2
  store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !2
  ret void
}
; CHECK-LABEL: Function: test_prp0_prp1:
; CHECK:  NoAlias:   store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !2 <->   store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2

; Function Attrs: nofree nounwind
define dso_local void @test_prS0_prS1(%struct.FOO* nocapture %_pS) local_unnamed_addr #0 !noalias !11 {
entry:
  %mpA = getelementptr inbounds %struct.FOO, %struct.FOO* %_pS, i32 0, i32 0
  %0 = load i32*, i32** %mpA, ptr_provenance i32** undef, align 4, !tbaa !14, !noalias !11
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %0, i8* null, i32** %mpA, i32** undef, i32 0, metadata !11), !tbaa !14, !noalias !11
  %mpB = getelementptr inbounds %struct.FOO, %struct.FOO* %_pS, i32 0, i32 1
  %2 = load i32*, i32** %mpB, ptr_provenance i32** undef, align 4, !tbaa !16, !noalias !11
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %2, i8* null, i32** nonnull %mpB, i32** undef, i32 0, metadata !11), !tbaa !16, !noalias !11
  store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !11
  store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !9, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test_prS0_prS1:
; CHECK:  NoAlias:   store i32 99, i32* %2, ptr_provenance i32* %3, align 4, !tbaa !11, !noalias !2 <->   store i32 42, i32* %0, ptr_provenance i32* %1, align 4, !tbaa !11, !noalias !2


; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #1

attributes #0 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_prp0_prp1: unknown scope"}
!4 = distinct !{!4, !"test_prp0_prp1"}
!5 = !{!6, !6, i64 0, i64 4}
!6 = !{!7, i64 4, !"any pointer"}
!7 = !{!8, i64 1, !"omnipotent char"}
!8 = !{!"Simple C/C++ TBAA"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!7, i64 4, !"int"}
!11 = !{!12}
!12 = distinct !{!12, !13, !"test_prS0_prS1: unknown scope"}
!13 = distinct !{!13, !"test_prS0_prS1"}
!14 = !{!15, !6, i64 0, i64 4}
!15 = !{!7, i64 8, !"FOO", !6, i64 0, i64 4, !6, i64 4, i64 4}
!16 = !{!15, !6, i64 4, i64 4}
