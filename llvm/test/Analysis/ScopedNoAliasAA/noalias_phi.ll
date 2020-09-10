; RUN: opt < %s -basic-aa -scoped-noalias-aa -aa-eval -evaluate-aa-metadata -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nofree norecurse nounwind writeonly
define dso_local void @test_phi_p_p_p(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #0 {
entry:
  %tobool = icmp eq i32 %c, 0
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* undef, align 4, !tbaa !2
  store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !2
  ret void
}
; CHECK-LABEL: Function: test_phi_p_p_p:
; CHECK:  MayAlias:   store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !2 <->   store i32 42, i32* %cond, ptr_provenance i32* undef, align 4, !tbaa !2

; Function Attrs: nounwind
define dso_local void @test_phi_rp_p_p(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !6)
  %tobool = icmp ne i32 %c, 0
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !6)
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  %prov.cond = phi i32* [ %1, %cond.true ], [ %_pB, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !2, !noalias !6
  store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !6
  ret void
}
; CHECK-LABEL: Function: test_phi_rp_p_p:
; CHECK:  MayAlias:   store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !5, !noalias !2 <->   store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !5, !noalias !2


; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #2

; Function Attrs: nounwind
define dso_local void @test_phi_p_rp_p(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !9)
  %tobool = icmp ne i32 %c, 0
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* %0, i32** null, i32** undef, i32 0, metadata !9)
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  %prov.cond = phi i32* [ %_pA, %cond.true ], [ %1, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !2, !noalias !9
  store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !9
  ret void
}
; CHECK-LABEL: Function: test_phi_p_rp_p:
; CHECK:  MayAlias:   store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !5, !noalias !2 <->   store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !5, !noalias !2

; Function Attrs: nounwind
define dso_local void @test_phi_rp_rp_p(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !12)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !15)
  %tobool = icmp ne i32 %c, 0
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !12)
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* %1, i32** null, i32** undef, i32 0, metadata !15)
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  %prov.cond = phi i32* [ %2, %cond.true ], [ %3, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !2, !noalias !17
  store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !17
  ret void
}
; CHECK-LABEL: Function: test_phi_rp_rp_p:
; CHECK:  NoAlias:   store i32 99, i32* %_pC, ptr_provenance i32* undef, align 4, !tbaa !7, !noalias !11 <->   store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !7, !noalias !11

; Function Attrs: nounwind
define dso_local void @test_phi_p_p_rp(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !18)
  %tobool = icmp eq i32 %c, 0
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !18
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pC, i8* %0, i32** null, i32** undef, i32 0, metadata !18), !tbaa !21, !noalias !18
  store i32 99, i32* %_pC, ptr_provenance i32* %1, align 4, !tbaa !2, !noalias !18
  ret void
}
; CHECK-LABEL: Function: test_phi_p_p_rp:
; CHECK:  NoAlias:   store i32 99, i32* %_pC, ptr_provenance i32* %1, align 4, !tbaa !5, !noalias !2 <->   store i32 42, i32* %cond, ptr_provenance i32* undef, align 4, !tbaa !5, !noalias !2

; Function Attrs: nounwind
define dso_local void @test_phi_rp_rp_rp_01(i32* nocapture %_pA, i32* nocapture %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !23)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !26)
  %2 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !28)
  %tobool = icmp ne i32 %c, 0
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !23)
  %4 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pB, i8* %1, i32** null, i32** undef, i32 0, metadata !26)
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pB, %cond.false ]
  %prov.cond = phi i32* [ %3, %cond.true ], [ %4, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !2, !noalias !30
  %5 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pC, i8* %2, i32** null, i32** undef, i32 0, metadata !28), !tbaa !21, !noalias !30
  store i32 99, i32* %_pC, ptr_provenance i32* %5, align 4, !tbaa !2, !noalias !30
  ret void
}

; CHECK-LABEL: Function: test_phi_rp_rp_rp_01:
; CHECK:  NoAlias:   store i32 99, i32* %_pC, ptr_provenance i32* %5, align 4, !tbaa !9, !noalias !13 <->   store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !9, !noalias !13

; Function Attrs: nounwind
define dso_local void @test_phi_rp_rp_rp_02(i32* nocapture %_pA, i32* nocapture readnone %_pB, i32* nocapture %_pC, i32 %c) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !31)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !34)
  %tobool = icmp ne i32 %c, 0
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !31)
  %3 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pC, i8* %1, i32** null, i32** undef, i32 0, metadata !34)
  br i1 %tobool, label %cond.false, label %cond.true

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  br label %cond.end

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi i32* [ %_pA, %cond.true ], [ %_pC, %cond.false ]
  %prov.cond = phi i32* [ %2, %cond.true ], [ %3, %cond.false ]
  store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !2, !noalias !36
  store i32 99, i32* %_pC, ptr_provenance i32* %3, align 4, !tbaa !2, !noalias !36
  ret void
}
; CHECK-LABEL: Function: test_phi_rp_rp_rp_02:
; CHECK:  MayAlias:   store i32 99, i32* %_pC, ptr_provenance i32* %3, align 4, !tbaa !7, !noalias !11 <->   store i32 42, i32* %cond, ptr_provenance i32* %prov.cond, align 4, !tbaa !7, !noalias !11

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #3

attributes #0 = { nofree norecurse nounwind writeonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3, !3, i64 0, i64 4}
!3 = !{!4, i64 4, !"int"}
!4 = !{!5, i64 1, !"omnipotent char"}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7}
!7 = distinct !{!7, !8, !"test_phi_rp_p_p: pA"}
!8 = distinct !{!8, !"test_phi_rp_p_p"}
!9 = !{!10}
!10 = distinct !{!10, !11, !"test_phi_p_rp_p: pB"}
!11 = distinct !{!11, !"test_phi_p_rp_p"}
!12 = !{!13}
!13 = distinct !{!13, !14, !"test_phi_rp_rp_p: pA"}
!14 = distinct !{!14, !"test_phi_rp_rp_p"}
!15 = !{!16}
!16 = distinct !{!16, !14, !"test_phi_rp_rp_p: pB"}
!17 = !{!13, !16}
!18 = !{!19}
!19 = distinct !{!19, !20, !"test_phi_p_p_rp: pC"}
!20 = distinct !{!20, !"test_phi_p_p_rp"}
!21 = !{!22, !22, i64 0, i64 4}
!22 = !{!4, i64 4, !"any pointer"}
!23 = !{!24}
!24 = distinct !{!24, !25, !"test_phi_rp_rp_rp_01: pA"}
!25 = distinct !{!25, !"test_phi_rp_rp_rp_01"}
!26 = !{!27}
!27 = distinct !{!27, !25, !"test_phi_rp_rp_rp_01: pB"}
!28 = !{!29}
!29 = distinct !{!29, !25, !"test_phi_rp_rp_rp_01: pC"}
!30 = !{!24, !27, !29}
!31 = !{!32}
!32 = distinct !{!32, !33, !"test_phi_rp_rp_rp_02: pA"}
!33 = distinct !{!33, !"test_phi_rp_rp_rp_02"}
!34 = !{!35}
!35 = distinct !{!35, !33, !"test_phi_rp_rp_rp_02: pC"}
!36 = !{!32, !37, !35}
!37 = distinct !{!37, !33, !"test_phi_rp_rp_rp_02: pB"}
