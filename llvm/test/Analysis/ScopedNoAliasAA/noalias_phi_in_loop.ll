; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind
define dso_local void @test_complex_phi_01(i32* nocapture %_pA, i32** nocapture readonly %_pB, i32 %n) local_unnamed_addr #0 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  %2 = load i32*, i32** %_pB, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx1 = getelementptr inbounds i32*, i32** %_pB, i32 1
  %3 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx2 = getelementptr inbounds i32*, i32** %_pB, i32 2
  %4 = load i32*, i32** %arrayidx2, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx3 = getelementptr inbounds i32*, i32** %_pB, i32 3
  %5 = load i32*, i32** %arrayidx3, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx4 = getelementptr inbounds i32*, i32** %_pB, i32 4
  %6 = load i32*, i32** %arrayidx4, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx5 = getelementptr inbounds i32*, i32** %_pB, i32 5
  %7 = load i32*, i32** %arrayidx5, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx6 = getelementptr inbounds i32*, i32** %_pB, i32 6
  %8 = load i32*, i32** %arrayidx6, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx7 = getelementptr inbounds i32*, i32** %_pB, i32 7
  %9 = load i32*, i32** %arrayidx7, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx8 = getelementptr inbounds i32*, i32** %_pB, i32 8
  %10 = load i32*, i32** %arrayidx8, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx9 = getelementptr inbounds i32*, i32** %_pB, i32 9
  %11 = load i32*, i32** %arrayidx9, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  br label %for.cond

for.cond:                                         ; preds = %for.cond, %entry
  %prov.pTmp00.0 = phi i32* [ %2, %entry ], [ %prov.pTmp01.0, %for.cond ]
  %pTmp00.0 = phi i32* [ %2, %entry ], [ %pTmp01.0, %for.cond ]
  %prov.pTmp01.0 = phi i32* [ %3, %entry ], [ %prov.pTmp02.0, %for.cond ]
  %pTmp01.0 = phi i32* [ %3, %entry ], [ %pTmp02.0, %for.cond ]
  %prov.pTmp02.0 = phi i32* [ %4, %entry ], [ %prov.pTmp03.0, %for.cond ]
  %pTmp02.0 = phi i32* [ %4, %entry ], [ %pTmp03.0, %for.cond ]
  %prov.pTmp03.0 = phi i32* [ %5, %entry ], [ %prov.pTmp04.0, %for.cond ]
  %pTmp03.0 = phi i32* [ %5, %entry ], [ %pTmp04.0, %for.cond ]
  %prov.pTmp04.0 = phi i32* [ %6, %entry ], [ %prov.pTmp05.0, %for.cond ]
  %pTmp04.0 = phi i32* [ %6, %entry ], [ %pTmp05.0, %for.cond ]
  %prov.pTmp05.0 = phi i32* [ %7, %entry ], [ %prov.pTmp06.0, %for.cond ]
  %pTmp05.0 = phi i32* [ %7, %entry ], [ %pTmp06.0, %for.cond ]
  %prov.pTmp06.0 = phi i32* [ %8, %entry ], [ %prov.pTmp07.0, %for.cond ]
  %pTmp06.0 = phi i32* [ %8, %entry ], [ %pTmp07.0, %for.cond ]
  %prov.pTmp07.0 = phi i32* [ %9, %entry ], [ %prov.pTmp08.0, %for.cond ]
  %pTmp07.0 = phi i32* [ %9, %entry ], [ %pTmp08.0, %for.cond ]
  %prov.pTmp08.0 = phi i32* [ %10, %entry ], [ %prov.pTmp09.0, %for.cond ]
  %pTmp08.0 = phi i32* [ %10, %entry ], [ %pTmp09.0, %for.cond ]
  %prov.pTmp09.0 = phi i32* [ %11, %entry ], [ %1, %for.cond ]
  %pTmp09.0 = phi i32* [ %11, %entry ], [ %_pA, %for.cond ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.cond ]
  %cmp = icmp slt i32 %i.0, %n
  %inc = add nuw nsw i32 %i.0, 1
  br i1 %cmp, label %for.cond, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond
  store i32 99, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2
  store i32 42, i32* %pTmp00.0, ptr_provenance i32* %prov.pTmp00.0, align 4, !tbaa !9, !noalias !2
  ret void
}

; CHECK-LABEL: Function: test_complex_phi_01:
; CHECK:  MayAlias:   store i32 42, i32* %pTmp00.0, ptr_provenance i32* %prov.pTmp00.0, align 4, !tbaa !9, !noalias !2 <->   store i32 99, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2


; Adapted version where the ptr_provenance chains don't collapse
; Function Attrs: nounwind
define dso_local void @test_complex_phi_02(i32* nocapture %_pA, i32** nocapture readonly %_pB, i32 %n) local_unnamed_addr #0 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %decl2 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 1, metadata !2)
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  %extra_ptr = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %decl2, i32** null, i32** undef, i32 1, metadata !2), !tbaa !5, !noalias !2
  %2 = load i32*, i32** %_pB, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx1 = getelementptr inbounds i32*, i32** %_pB, i32 1
  %3 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx2 = getelementptr inbounds i32*, i32** %_pB, i32 2
  %4 = load i32*, i32** %arrayidx2, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx3 = getelementptr inbounds i32*, i32** %_pB, i32 3
  %5 = load i32*, i32** %arrayidx3, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx4 = getelementptr inbounds i32*, i32** %_pB, i32 4
  %6 = load i32*, i32** %arrayidx4, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx5 = getelementptr inbounds i32*, i32** %_pB, i32 5
  %7 = load i32*, i32** %arrayidx5, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx6 = getelementptr inbounds i32*, i32** %_pB, i32 6
  %8 = load i32*, i32** %arrayidx6, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx7 = getelementptr inbounds i32*, i32** %_pB, i32 7
  %9 = load i32*, i32** %arrayidx7, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx8 = getelementptr inbounds i32*, i32** %_pB, i32 8
  %10 = load i32*, i32** %arrayidx8, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  %arrayidx9 = getelementptr inbounds i32*, i32** %_pB, i32 9
  %11 = load i32*, i32** %arrayidx9, ptr_provenance i32** undef, align 4, !tbaa !5, !noalias !2
  br label %for.cond

for.cond:                                         ; preds = %for.cond, %entry
  %prov.pTmp00.0 = phi i32* [ %2, %entry ], [ %prov.pTmp01.0, %for.cond ]
  %pTmp00.0 = phi i32* [ %2, %entry ], [ %pTmp01.0, %for.cond ]
  %prov.pTmp01.0 = phi i32* [ %3, %entry ], [ %prov.pTmp02.0, %for.cond ]
  %pTmp01.0 = phi i32* [ %3, %entry ], [ %pTmp02.0, %for.cond ]
  %prov.pTmp02.0 = phi i32* [ %4, %entry ], [ %prov.pTmp03.0, %for.cond ]
  %pTmp02.0 = phi i32* [ %4, %entry ], [ %pTmp03.0, %for.cond ]
  %prov.pTmp03.0 = phi i32* [ %5, %entry ], [ %prov.pTmp04.0, %for.cond ]
  %pTmp03.0 = phi i32* [ %5, %entry ], [ %pTmp04.0, %for.cond ]
  %prov.pTmp04.0 = phi i32* [ %6, %entry ], [ %prov.pTmp05.0, %for.cond ]
  %pTmp04.0 = phi i32* [ %6, %entry ], [ %pTmp05.0, %for.cond ]
  %prov.pTmp05.0 = phi i32* [ %7, %entry ], [ %prov.pTmp06.0, %for.cond ]
  %pTmp05.0 = phi i32* [ %7, %entry ], [ %pTmp06.0, %for.cond ]
  %prov.pTmp06.0 = phi i32* [ %8, %entry ], [ %prov.pTmp07.0, %for.cond ]
  %pTmp06.0 = phi i32* [ %8, %entry ], [ %pTmp07.0, %for.cond ]
  %prov.pTmp07.0 = phi i32* [ %9, %entry ], [ %prov.pTmp08.0, %for.cond ]
  %pTmp07.0 = phi i32* [ %9, %entry ], [ %pTmp08.0, %for.cond ]
  %prov.pTmp08.0 = phi i32* [ %10, %entry ], [ %prov.pTmp09.0, %for.cond ]
  %pTmp08.0 = phi i32* [ %10, %entry ], [ %pTmp09.0, %for.cond ]
  %prov.pTmp09.0 = phi i32* [ %11, %entry ], [ %extra_ptr, %for.cond ]
  %pTmp09.0 = phi i32* [ %11, %entry ], [ %_pA, %for.cond ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.cond ]
  %cmp = icmp slt i32 %i.0, %n
  %inc = add nuw nsw i32 %i.0, 1
  br i1 %cmp, label %for.cond, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond
  store i32 99, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2
  store i32 42, i32* %pTmp00.0, ptr_provenance i32* %prov.pTmp00.0, align 4, !tbaa !9, !noalias !2
  ret void
}

; CHECK-LABEL: Function: test_complex_phi_02:
; CHECK:  NoAlias:   store i32 42, i32* %pTmp00.0, ptr_provenance i32* %prov.pTmp00.0, align 4, !tbaa !9, !noalias !2 <->   store i32 99, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #1

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #2

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_complex_phi: rpTmp"}
!4 = distinct !{!4, !"test_complex_phi"}
!5 = !{!6, !6, i64 0, i64 4}
!6 = !{!7, i64 4, !"any pointer"}
!7 = !{!8, i64 1, !"omnipotent char"}
!8 = !{!"Simple C/C++ TBAA"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!7, i64 4, !"int"}
