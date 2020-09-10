; RUN: opt < %s -convert-noalias -verify -S | FileCheck %s
; RUN: opt < %s -passes=convert-noalias,verify -S | FileCheck %s

target datalayout = "e-p:64:64:64-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-n8:16:32:64"

; Function Attrs: nounwind
define dso_local i32* @passP(i32* %_pA) #0 {
entry:
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %1 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %1, align 4, !tbaa !9, !noalias !2
  %2 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !2), !tbaa !5, !noalias !2
  ret i32* %2
}

; CHECK-LABEL:  @passP(
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
; CHECK-NEXT:   %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
; CHECK-NEXT:   store i32 42, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !2
; CHECK-NEXT:   %.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %1)
; CHECK-NEXT:   ret i32* %.guard
; CHECK-NEXT: }

; Function Attrs: nounwind
define dso_local void @test01(i32* %_pA) #0 {
entry:
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
  %1 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !11), !tbaa !5, !noalias !11
  store i32 41, i32* %1, align 4, !tbaa !9, !noalias !11
  %2 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !11), !tbaa !5, !noalias !11
  %call = call i32* @passP(i32* %2), !noalias !11
  store i32 43, i32* %call, align 4, !tbaa !9, !noalias !11
  ret void
}

; CHECK-LABEL: @test01(
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
; CHECK-NEXT:   %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !11), !tbaa !5, !noalias !11
; CHECK-NEXT:   store i32 41, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !11
; CHECK-NEXT:   %.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %1)
; CHECK-NEXT:   %call = call i32* @passP(i32* %.guard), !noalias !11
; CHECK-NEXT:   store i32 43, i32* %call, align 4, !tbaa !9, !noalias !11
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

; Function Attrs: nounwind
define dso_local void @test02(i32* %_pA) #0 {
entry:
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
  %1 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !11), !tbaa !5, !noalias !11
  store i32 41, i32* %1, align 4, !tbaa !9, !noalias !11
  %2 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32 0, metadata !11), !tbaa !5, !noalias !11
  br label %block

block:
  %tmp0 = phi i32* [ %2, %entry ]
  %tmp1 = phi i32* [ %1, %entry ]
  %call = call i32* @passP(i32* %tmp0), !noalias !11
  store i32 43, i32* %call, align 4, !tbaa !9, !noalias !11
  ret void
}


; CHECK-LABEL: @test02(
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
; CHECK-NEXT:   %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !11), !tbaa !5, !noalias !11
; CHECK-NEXT:   store i32 41, i32* %_pA, ptr_provenance i32* %1, align 4, !tbaa !9, !noalias !11
; CHECK-NEXT:   br label %block
; CHECK: block:
; CHECK-NEXT:   %prov.tmp0 = phi i32* [ %1, %entry ]
; CHECK-NEXT:   %tmp0 = phi i32* [ %_pA, %entry ]
; CHECK-NEXT:   %prov.tmp1 = phi i32* [ %1, %entry ]
; CHECK-NEXT:   %tmp1 = phi i32* [ %_pA, %entry ]
; CHECK-NEXT:   %tmp0.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %tmp0, i32* %prov.tmp0)
; CHECK-NEXT:   %call = call i32* @passP(i32* %tmp0.guard), !noalias !11
; CHECK-NEXT:   store i32 43, i32* %call, align 4, !tbaa !9, !noalias !11
; CHECK-NEXT:   ret void
; CHECK-NEXT: }

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #2

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) #3

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }
attributes #2 = { argmemonly nounwind }
attributes #3 = { argmemonly nounwind speculatable }
attributes #4 = { nounwind readnone speculatable }
attributes #5 = { nounwind readnone }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"passP: pA"}
!4 = distinct !{!4, !"passP"}
!5 = !{!6, !6, i64 0, i64 4}
!6 = !{!7, i64 4, !"any pointer"}
!7 = !{!8, i64 1, !"omnipotent char"}
!8 = !{!"Simple C/C++ TBAA"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!7, i64 4, !"int"}
!11 = !{!12}
!12 = distinct !{!12, !13, !"test01: p1"}
!13 = distinct !{!13, !"test01"}
