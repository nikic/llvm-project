; RUN: opt < %s -convert-noalias -verify -S | FileCheck %s
; RUN: opt < %s -passes=convert-noalias,verify -S | FileCheck %s

target datalayout = "e-i8:8:8-i16:16:16-i32:32:32-i64:32:32-f16:16:16-f32:32:32-f64:32:32-p:32:32:32:32:8-s0:32:32-a0:0:32-S32-n16:32-v128:32:32-P0-p0:32:32:32:32:8"

; Function Attrs: nounwind
define dso_local void @test_rr(i32** %_p) #0 !noalias !2 {
entry:
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i32(i32*** null, i32 0, metadata !5)
  %1 = call i32** @llvm.noalias.p0p0i32.p0i8.p0p0p0i32.i32(i32** %_p, i8* %0, i32*** null, i32 0, metadata !5), !tbaa !7, !noalias !11
  %arrayidx = getelementptr inbounds i32*, i32** %1, i32 1
  %2 = load i32*, i32** %arrayidx, align 4, !tbaa !7, !noalias !11
  %3 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %2, i8* null, i32** %arrayidx, i32 0, metadata !2), !tbaa !7, !noalias !11
  %arrayidx1 = getelementptr inbounds i32, i32* %3, i32 2
  %4 = load i32, i32* %arrayidx1, align 4, !tbaa !12, !noalias !11
  %add = add nsw i32 %4, 1
  %5 = call i32** @llvm.noalias.p0p0i32.p0i8.p0p0p0i32.i32(i32** %_p, i8* %0, i32*** null, i32 0, metadata !5), !tbaa !7, !noalias !11
  %6 = load i32*, i32** %5, align 4, !tbaa !7, !noalias !11
  %7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %6, i8* null, i32** %5, i32 0, metadata !2), !tbaa !7, !noalias !11
  store i32 %add, i32* %7, align 4, !tbaa !12, !noalias !11
  ret void
}

; CHECK-LABEL: @test_rr(
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i32(i32*** null, i32 0, metadata !5)
; CHECK-NEXT:   %1 = call i32** @llvm.provenance.noalias.p0p0i32.p0i8.p0p0p0i32.p0p0p0i32.i32(i32** %_p, i8* %0, i32*** null, i32*** undef, i32 0, metadata !5), !tbaa !7, !noalias !11
; CHECK-NEXT:   %arrayidx = getelementptr inbounds i32*, i32** %_p, i32 1
; CHECK-NEXT:   %2 = load i32*, i32** %arrayidx, ptr_provenance i32** %1, align 4, !tbaa !7, !noalias !11
; CHECK-NEXT:   %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %2, i8* null, i32** %arrayidx, i32** %1, i32 0, metadata !2), !tbaa !7, !noalias !11
; CHECK-NEXT:   %arrayidx1 = getelementptr inbounds i32, i32* %2, i32 2
; CHECK-NEXT:   %4 = load i32, i32* %arrayidx1, ptr_provenance i32* %3, align 4, !tbaa !12, !noalias !11
; CHECK-NEXT:   %add = add nsw i32 %4, 1
; CHECK-NEXT:   %5 = load i32*, i32** %_p, ptr_provenance i32** %1, align 4, !tbaa !7, !noalias !11
; CHECK-NEXT:   %6 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %5, i8* null, i32** %_p, i32** %1, i32 0, metadata !2), !tbaa !7, !noalias !11
; CHECK-NEXT:   store i32 %add, i32* %5, ptr_provenance i32* %6, align 4, !tbaa !12, !noalias !11
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0p0i32.i32(i32***, i32, metadata) #2

; Function Attrs: argmemonly nounwind speculatable
declare i32** @llvm.noalias.p0p0i32.p0i8.p0p0p0i32.i32(i32**, i8*, i32***, i32, metadata) #1

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind speculatable }
attributes #2 = { argmemonly nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_rr: unknown scope"}
!4 = distinct !{!4, !"test_rr"}
!5 = !{!6}
!6 = distinct !{!6, !4, !"test_rr: rprp"}
!7 = !{!8, !8, i64 0, i64 4}
!8 = !{!9, i64 4, !"any pointer"}
!9 = !{!10, i64 1, !"omnipotent char"}
!10 = !{!"Simple C/C++ TBAA"}
!11 = !{!6, !3}
!12 = !{!13, !13, i64 0, i64 4}
!13 = !{!9, i64 4, !"int"}
