; RUN: opt -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=1 -S < %s | FileCheck %s -check-prefix=INTR-SCOPE
; verify that inlining result in scope duplication
; verify that llvm.noalias.decl is introduced at the location of the inlining

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nofree norecurse nounwind
define dso_local void @copy_npnp(i32* noalias nocapture %dst, i32* noalias nocapture readonly %src) local_unnamed_addr #0 {
entry:
  %0 = load i32, i32* %src, ptr_provenance i32* undef, align 4, !tbaa !2
  store i32 %0, i32* %dst, ptr_provenance i32* undef, align 4, !tbaa !2
  ret void
}

; Function Attrs: nounwind
define dso_local void @copy_rprp(i32* nocapture %dst, i32* nocapture readonly %src) local_unnamed_addr #1 {
entry:
  %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !6)
  %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !9)
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %src, i8* %1, i32** null, i32** undef, i64 0, metadata !9), !tbaa !11, !noalias !13
  %3 = load i32, i32* %src, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !13
  %4 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %dst, i8* %0, i32** null, i32** undef, i64 0, metadata !6), !tbaa !11, !noalias !13
  store i32 %3, i32* %dst, ptr_provenance i32* %4, align 4, !tbaa !2, !noalias !13
  ret void
}

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32**, i64, metadata) #2

; Function Attrs: nofree norecurse nounwind
define dso_local void @test_npnp(i32* nocapture %dst, i32* nocapture readonly %src, i32 %n) local_unnamed_addr #0 {
entry:
  tail call void @copy_npnp(i32* %dst, i32* %src)
  br label %do.body

do.body:                                          ; preds = %do.body, %entry
  %n.addr.0 = phi i32 [ %n, %entry ], [ %dec, %do.body ]
  tail call void @copy_npnp(i32* %dst, i32* %src)
  tail call void @copy_npnp(i32* %dst, i32* %src)
  %dec = add nsw i32 %n.addr.0, -1
  %tobool = icmp eq i32 %n.addr.0, 0
  br i1 %tobool, label %do.end, label %do.body

do.end:                                           ; preds = %do.body
  ret void
}

; INTR-SCOPE: define dso_local void @test_npnp(i32* nocapture %dst, i32* nocapture readonly %src, i32 %n) local_unnamed_addr #0 {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !14)
; INTR-SCOPE:   %1 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %dst, i8* %0, i32** null, i64 0, metadata !14)
; INTR-SCOPE:   %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !17)
; INTR-SCOPE:   %3 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %src, i8* %2, i32** null, i64 0, metadata !17)
; INTR-SCOPE:   %4 = load i32, i32* %3, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !19
; INTR-SCOPE:   store i32 %4, i32* %1, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !19
; INTR-SCOPE:   br label %do.body
; INTR-SCOPE: do.body:                                          ; preds = %do.body, %entry
; INTR-SCOPE:   %n.addr.0 = phi i32 [ %n, %entry ], [ %dec, %do.body ]
; INTR-SCOPE:   %5 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !20)
; INTR-SCOPE:   %6 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %dst, i8* %5, i32** null, i64 0, metadata !20)
; INTR-SCOPE:   %7 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !23)
; INTR-SCOPE:   %8 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %src, i8* %7, i32** null, i64 0, metadata !23)
; INTR-SCOPE:   %9 = load i32, i32* %8, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !25
; INTR-SCOPE:   store i32 %9, i32* %6, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !25
; INTR-SCOPE:   %10 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !26)
; INTR-SCOPE:   %11 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %dst, i8* %10, i32** null, i64 0, metadata !26)
; INTR-SCOPE:   %12 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !29)
; INTR-SCOPE:   %13 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %src, i8* %12, i32** null, i64 0, metadata !29)
; INTR-SCOPE:   %14 = load i32, i32* %13, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !31
; INTR-SCOPE:   store i32 %14, i32* %11, ptr_provenance i32* undef, align 4, !tbaa !2, !noalias !31
; INTR-SCOPE:   %dec = add nsw i32 %n.addr.0, -1
; INTR-SCOPE:   %tobool = icmp eq i32 %n.addr.0, 0
; INTR-SCOPE:   br i1 %tobool, label %do.end, label %do.body
; INTR-SCOPE: do.end:                                           ; preds = %do.body
; INTR-SCOPE:   ret void
; INTR-SCOPE: }


; Function Attrs: nounwind
define dso_local void @test_rprp(i32* nocapture %dst, i32* nocapture readonly %src, i32 %n) local_unnamed_addr #1 {
entry:
  tail call void @copy_rprp(i32* %dst, i32* %src)
  br label %do.body

do.body:                                          ; preds = %do.body, %entry
  %n.addr.0 = phi i32 [ %n, %entry ], [ %dec, %do.body ]
  tail call void @copy_rprp(i32* %dst, i32* %src)
  tail call void @copy_rprp(i32* %dst, i32* %src)
  %dec = add nsw i32 %n.addr.0, -1
  %tobool = icmp eq i32 %n.addr.0, 0
  br i1 %tobool, label %do.end, label %do.body

do.end:                                           ; preds = %do.body
  ret void
}

; INTR-SCOPE: define dso_local void @test_rprp(i32* nocapture %dst, i32* nocapture readonly %src, i32 %n) local_unnamed_addr #1 {
; INTR-SCOPE: entry:
; INTR-SCOPE:   %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !32) #5
; INTR-SCOPE:   %1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !35) #5
; INTR-SCOPE:   %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %src, i8* %1, i32** null, i32** undef, i64 0, metadata !35) #5, !tbaa !11, !noalias !37
; INTR-SCOPE:   %3 = load i32, i32* %src, ptr_provenance i32* %2, align 4, !tbaa !2, !noalias !37
; INTR-SCOPE:   %4 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %dst, i8* %0, i32** null, i32** undef, i64 0, metadata !32) #5, !tbaa !11, !noalias !37
; INTR-SCOPE:   store i32 %3, i32* %dst, ptr_provenance i32* %4, align 4, !tbaa !2, !noalias !37
; INTR-SCOPE:   br label %do.body
; INTR-SCOPE: do.body:                                          ; preds = %do.body, %entry
; INTR-SCOPE:   %n.addr.0 = phi i32 [ %n, %entry ], [ %dec, %do.body ]
; INTR-SCOPE:   %5 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !38) #5
; INTR-SCOPE:   %6 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !41) #5
; INTR-SCOPE:   %7 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %src, i8* %6, i32** null, i32** undef, i64 0, metadata !41) #5, !tbaa !11, !noalias !43
; INTR-SCOPE:   %8 = load i32, i32* %src, ptr_provenance i32* %7, align 4, !tbaa !2, !noalias !43
; INTR-SCOPE:   %9 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %dst, i8* %5, i32** null, i32** undef, i64 0, metadata !38) #5, !tbaa !11, !noalias !43
; INTR-SCOPE:   store i32 %8, i32* %dst, ptr_provenance i32* %9, align 4, !tbaa !2, !noalias !43
; INTR-SCOPE:   %10 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !44) #5
; INTR-SCOPE:   %11 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !47) #5
; INTR-SCOPE:   %12 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %src, i8* %11, i32** null, i32** undef, i64 0, metadata !47) #5, !tbaa !11, !noalias !49
; INTR-SCOPE:   %13 = load i32, i32* %src, ptr_provenance i32* %12, align 4, !tbaa !2, !noalias !49
; INTR-SCOPE:   %14 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %dst, i8* %10, i32** null, i32** undef, i64 0, metadata !44) #5, !tbaa !11, !noalias !49
; INTR-SCOPE:   store i32 %13, i32* %dst, ptr_provenance i32* %14, align 4, !tbaa !2, !noalias !49
; INTR-SCOPE:   %dec = add nsw i32 %n.addr.0, -1
; INTR-SCOPE:   %tobool = icmp eq i32 %n.addr.0, 0
; INTR-SCOPE:   br i1 %tobool, label %do.end, label %do.body
; INTR-SCOPE: do.end:                                           ; preds = %do.body
; INTR-SCOPE:   ret void
; INTR-SCOPE: }


; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32*, i8*, i32**, i32**, i64, metadata) #3

attributes #0 = { nofree norecurse nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
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
!7 = distinct !{!7, !8, !"copy_rprp: rdst"}
!8 = distinct !{!8, !"copy_rprp"}
!9 = !{!10}
!10 = distinct !{!10, !8, !"copy_rprp: rsrc"}
!11 = !{!12, !12, i64 0, i64 4}
!12 = !{!4, i64 4, !"any pointer"}
!13 = !{!7, !10}

; INTR-SCOPE: !0 = !{i32 1, !"wchar_size", i32 4}
; INTR-SCOPE: !1 = !{!"clang"}
; INTR-SCOPE: !2 = !{!3, !3, i64 0, i64 4}
; INTR-SCOPE: !3 = !{!4, i64 4, !"int"}
; INTR-SCOPE: !4 = !{!5, i64 1, !"omnipotent char"}
; INTR-SCOPE: !5 = !{!"Simple C/C++ TBAA"}
; INTR-SCOPE: !6 = !{!7}
; INTR-SCOPE: !7 = distinct !{!7, !8, !"copy_rprp: rdst"}
; INTR-SCOPE: !8 = distinct !{!8, !"copy_rprp"}
; INTR-SCOPE: !9 = !{!10}
; INTR-SCOPE: !10 = distinct !{!10, !8, !"copy_rprp: rsrc"}
; INTR-SCOPE: !11 = !{!12, !12, i64 0, i64 4}
; INTR-SCOPE: !12 = !{!4, i64 4, !"any pointer"}
; INTR-SCOPE: !13 = !{!7, !10}
; INTR-SCOPE: !14 = !{!15}
; INTR-SCOPE: !15 = distinct !{!15, !16, !"copy_npnp: %dst"}
; INTR-SCOPE: !16 = distinct !{!16, !"copy_npnp"}
; INTR-SCOPE: !17 = !{!18}
; INTR-SCOPE: !18 = distinct !{!18, !16, !"copy_npnp: %src"}
; INTR-SCOPE: !19 = !{!15, !18}
; INTR-SCOPE: !20 = !{!21}
; INTR-SCOPE: !21 = distinct !{!21, !22, !"copy_npnp: %dst"}
; INTR-SCOPE: !22 = distinct !{!22, !"copy_npnp"}
; INTR-SCOPE: !23 = !{!24}
; INTR-SCOPE: !24 = distinct !{!24, !22, !"copy_npnp: %src"}
; INTR-SCOPE: !25 = !{!21, !24}
; INTR-SCOPE: !26 = !{!27}
; INTR-SCOPE: !27 = distinct !{!27, !28, !"copy_npnp: %dst"}
; INTR-SCOPE: !28 = distinct !{!28, !"copy_npnp"}
; INTR-SCOPE: !29 = !{!30}
; INTR-SCOPE: !30 = distinct !{!30, !28, !"copy_npnp: %src"}
; INTR-SCOPE: !31 = !{!27, !30}
; INTR-SCOPE: !32 = !{!33}
; INTR-SCOPE: !33 = distinct !{!33, !34, !"copy_rprp: rdst"}
; INTR-SCOPE: !34 = distinct !{!34, !"copy_rprp"}
; INTR-SCOPE: !35 = !{!36}
; INTR-SCOPE: !36 = distinct !{!36, !34, !"copy_rprp: rsrc"}
; INTR-SCOPE: !37 = !{!33, !36}
; INTR-SCOPE: !38 = !{!39}
; INTR-SCOPE: !39 = distinct !{!39, !40, !"copy_rprp: rdst"}
; INTR-SCOPE: !40 = distinct !{!40, !"copy_rprp"}
; INTR-SCOPE: !41 = !{!42}
; INTR-SCOPE: !42 = distinct !{!42, !40, !"copy_rprp: rsrc"}
; INTR-SCOPE: !43 = !{!39, !42}
; INTR-SCOPE: !44 = !{!45}
; INTR-SCOPE: !45 = distinct !{!45, !46, !"copy_rprp: rdst"}
; INTR-SCOPE: !46 = distinct !{!46, !"copy_rprp"}
; INTR-SCOPE: !47 = !{!48}
; INTR-SCOPE: !48 = distinct !{!48, !46, !"copy_rprp: rsrc"}
; INTR-SCOPE: !49 = !{!45, !48}
