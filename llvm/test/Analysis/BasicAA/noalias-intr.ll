; RUN: opt < %s -basic-aa -aa-eval -print-all-alias-modref-info -disable-output 2>&1 | FileCheck %s

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

; Function Attrs: nounwind
define void @test01() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %t5 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %t1, i32** null, i32 0, metadata !2), !tbaa !7, !noalias !11
  store i32 42, i32* %t5, align 4, !tbaa !12, !noalias !11
  %t7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pB, i8* %t3, i32** null, i32 0, metadata !5), !tbaa !7, !noalias !11
  store i32 43, i32* %t7, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test01:
; CHECK: NoAlias:      i32* %t5, i32* %t7

; Function Attrs: nounwind
define void @test02() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %t5 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %t1, i32** null, i32 0, metadata !2), !tbaa !7, !noalias !11
  store i32 42, i32* %t5, align 4, !tbaa !12, !noalias !11
  %t7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %t3, i32** null, i32 0, metadata !5), !tbaa !7, !noalias !11
  store i32 43, i32* %t7, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test02:
; CHECK: MustAlias:      i32* %t5, i32* %t7


; Function Attrs: nounwind
define void @test11() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %t5 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %t1, i32** null, i32** undef, i32 0, metadata !2), !tbaa !7, !noalias !11
  %.guard1 = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %t5)
  store i32 42, i32* %.guard1, align 4, !tbaa !12, !noalias !11
  %.guard2 = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pB, i32* %t5)
  store i32 43, i32* %.guard2, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test11:
; CHECK: NoAlias:      i32* %.guard1, i32* %.guard2

; Function Attrs: nounwind
define void @test12() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %t5 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %t1, i32** null, i32** undef, i32 0, metadata !2), !tbaa !7, !noalias !11
  %.guard1 = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %t5)
  store i32 42, i32* %.guard1, align 4, !tbaa !12, !noalias !11
  %.guard2 = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %t5)
  store i32 43, i32* %.guard2, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test12:
; CHECK: MustAlias:      i32* %.guard1, i32* %.guard2

; Function Attrs: nounwind
define void @test21() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %.guard1 = call i32* @llvm.noalias.copy.guard.p0i32.p0i8(i32* %_pA, i8* %t1, metadata !14, metadata !2)
  store i32 42, i32* %.guard1, align 4, !tbaa !12, !noalias !11
  %.guard2 = call i32* @llvm.noalias.copy.guard.p0i32.p0i8(i32* %_pB, i8* %t3, metadata !14, metadata !5)
  store i32 43, i32* %.guard2, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test21:
; CHECK: NoAlias:      i32* %.guard1, i32* %.guard2

; Function Attrs: nounwind
define void @test22() #0 {
entry:
  %_pA = alloca i32, align 4
  %_pB = alloca i32, align 4
  %t1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %t3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !5)
  %.guard1 = call i32* @llvm.noalias.copy.guard.p0i32.p0i8(i32* %_pA, i8* %t1, metadata !14, metadata !2)
  store i32 42, i32* %.guard1, align 4, !tbaa !12, !noalias !11
  %.guard2 = call i32* @llvm.noalias.copy.guard.p0i32.p0i8(i32* %_pA, i8* %t3, metadata !14, metadata !5)
  store i32 43, i32* %.guard2, align 4, !tbaa !12, !noalias !11
  ret void
}
; CHECK-LABEL: Function: test22:
; CHECK: MustAlias:      i32* %.guard1, i32* %.guard2

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #1

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) #2

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #3

; Function Attrs: nounwind readnone
declare i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32*, i32*) #4

; Function Attrs: nounwind readnone
declare i32* @llvm.noalias.copy.guard.p0i32.p0i8(i32*, i8*, metadata, metadata) #4

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { argmemonly nounwind speculatable }
attributes #3 = { nounwind readnone speculatable }
attributes #4 = { nounwind readnone }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test01: _pA"}
!4 = distinct !{!4, !"test01"}
!5 = !{!6}
!6 = distinct !{!6, !4, !"test01: _pB"}
!7 = !{!8, !8, i64 0, i64 4}
!8 = !{!9, i64 4, !"any pointer"}
!9 = !{!10, i64 1, !"omnipotent char"}
!10 = !{!"Simple C/C++ TBAA"}
!11 = !{!3, !6}
!12 = !{!13, !13, i64 0, i64 4}
!13 = !{!9, i64 4, !"int"}
!14 = !{!15}
!15 = !{i64 -1, i64 0}
