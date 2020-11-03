; RUN: opt < %s -convert-noalias -verify -S | FileCheck %s
; RUN: opt < %s -passes=convert-noalias,verify -S | FileCheck %s
target datalayout = "e-i8:8:8-i16:16:16-i32:32:32-i64:32:32-f16:16:16-f32:32:32-f64:32:32-p:32:32:32:32:8-s0:32:32-a0:0:32-S32-n16:32-v128:32:32-P0-p0:32:32:32:32:8"

%struct.FOO = type { i32*, i32*, i32* }

; Function Attrs: nounwind
define dso_local void @test_rFOO(%struct.FOO* %_pFOO, i32* %_pA) #0 !noalias !2 {
entry:
  %tmp = alloca %struct.FOO, align 4
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0s_struct.FOOs.i32(%struct.FOO** null, i32 0, metadata !5)
  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !7)
  %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 4, metadata !7)
  %3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 8, metadata !7)
  %4 = call %struct.FOO* @llvm.noalias.p0s_struct.FOOs.p0i8.p0p0s_struct.FOOs.i32(%struct.FOO* %_pFOO, i8* %0, %struct.FOO** null, i32 0, metadata !5), !tbaa !9, !noalias !13
  %5 = call %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO* %4, i8* null, metadata !14, metadata !2)
  %6 = load %struct.FOO, %struct.FOO* %5, align 4, !noalias !13
  %.fca.0.extract = extractvalue %struct.FOO %6, 0
  %.fca.1.extract = extractvalue %struct.FOO %6, 1
  %.fca.2.extract = extractvalue %struct.FOO %6, 2
  %7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %.fca.0.extract, i8* %1, i32** null, i32 0, metadata !7), !tbaa !18, !noalias !13
  %8 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %.fca.1.extract, i8* %2, i32** null, i32 4, metadata !7), !tbaa !20, !noalias !13
  %9 = load i32, i32* %8, align 4, !tbaa !21, !noalias !13
  store i32 %9, i32* %7, align 4, !tbaa !21, !noalias !13
  %10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %1, i32** null, i32 0, metadata !7), !tbaa !18, !noalias !13
  store i32 42, i32* %10, align 4, !tbaa !21, !noalias !13
  %.fca.0.load.noalias = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %_pA, i8* %1, i32** null, i32 0, metadata !7)
  %.fca.0.insert = insertvalue %struct.FOO undef, i32* %.fca.0.load.noalias, 0
  %.fca.1.load.noalias = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %.fca.1.extract, i8* %2, i32** null, i32 4, metadata !7)
  %.fca.1.insert = insertvalue %struct.FOO %.fca.0.insert, i32* %.fca.1.load.noalias, 1
  %.fca.2.load.noalias = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %.fca.2.extract, i8* %3, i32** null, i32 8, metadata !7)
  %.fca.2.insert = insertvalue %struct.FOO %.fca.1.insert, i32* %.fca.2.load.noalias, 2
  call void @fum(%struct.FOO %.fca.2.insert), !noalias !13
  ret void
}

; CHECK-LABEL: @test_rFOO(
; CHECK-NEXT: entry:
; CHECK-NEXT:   %tmp = alloca %struct.FOO, align 4
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0s_struct.FOOs.i32(%struct.FOO** null, i32 0, metadata !5)
; CHECK-NEXT:   %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !7)
; CHECK-NEXT:   %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 4, metadata !7)
; CHECK-NEXT:   %3 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 8, metadata !7)
; CHECK-NEXT:   %4 = call %struct.FOO* @llvm.provenance.noalias.p0s_struct.FOOs.p0i8.p0p0s_struct.FOOs.p0p0s_struct.FOOs.i32(%struct.FOO* %_pFOO, i8* %0, %struct.FOO** null, %struct.FOO** undef, i32 0, metadata !5), !tbaa !9, !noalias !13
; CHECK-NEXT:   %.guard = call %struct.FOO* @llvm.noalias.arg.guard.p0s_struct.FOOs.p0s_struct.FOOs(%struct.FOO* %_pFOO, %struct.FOO* %4)
; CHECK-NEXT:   %5 = call %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO* %.guard, i8* null, metadata !14, metadata !2)
; CHECK-NEXT:   %6 = load %struct.FOO, %struct.FOO* %5, align 4, !noalias !13
; CHECK-NEXT:   %.fca.0.extract = extractvalue %struct.FOO %6, 0
; CHECK-NEXT:   %.fca.1.extract = extractvalue %struct.FOO %6, 1
; CHECK-NEXT:   %.fca.2.extract = extractvalue %struct.FOO %6, 2
; CHECK-NEXT:   %7 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %.fca.0.extract, i8* %1, i32** null, i32** undef, i32 0, metadata !7), !tbaa !18, !noalias !13
; CHECK-NEXT:   %8 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %.fca.1.extract, i8* %2, i32** null, i32** undef, i32 4, metadata !7), !tbaa !20, !noalias !13
; CHECK-NEXT:   %9 = load i32, i32* %.fca.1.extract, ptr_provenance i32* %8, align 4, !tbaa !21, !noalias !13
; CHECK-NEXT:   store i32 %9, i32* %.fca.0.extract, ptr_provenance i32* %7, align 4, !tbaa !21, !noalias !13
; CHECK-NEXT:   %10 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %1, i32** null, i32** undef, i32 0, metadata !7), !tbaa !18, !noalias !13
; CHECK-NEXT:   store i32 42, i32* %_pA, ptr_provenance i32* %10, align 4, !tbaa !21, !noalias !13
; CHECK-NEXT:   %.fca.0.load.noalias.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %_pA, i32* %10)
; CHECK-NEXT:   %.fca.0.insert = insertvalue %struct.FOO undef, i32* %.fca.0.load.noalias.guard, 0
; CHECK-NEXT:   %.fca.1.load.noalias.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %.fca.1.extract, i32* %8)
; CHECK-NEXT:   %.fca.1.insert = insertvalue %struct.FOO %.fca.0.insert, i32* %.fca.1.load.noalias.guard, 1
; CHECK-NEXT:   %11 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %.fca.2.extract, i8* %3, i32** null, i32** undef, i32 8, metadata !7)
; CHECK-NEXT:   %.fca.2.load.noalias.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %.fca.2.extract, i32* %11)
; CHECK-NEXT:   %.fca.2.insert = insertvalue %struct.FOO %.fca.1.insert, i32* %.fca.2.load.noalias.guard, 2
; CHECK-NEXT:   call void @fum(%struct.FOO %.fca.2.insert), !noalias !13
; CHECK-NEXT:   ret void
; CHECK-NEXT: }


; Function Attrs: argmemonly nounwind speculatable
declare %struct.FOO* @llvm.noalias.p0s_struct.FOOs.p0i8.p0p0s_struct.FOOs.i32(%struct.FOO*, i8*, %struct.FOO**, i32, metadata) #1

; Function Attrs: nounwind readnone
declare %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO*, i8*, metadata, metadata) #2

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #3

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0s_struct.FOOs.i32(%struct.FOO**, i32, metadata) #3

declare dso_local void @fum(%struct.FOO) #4

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind speculatable }
attributes #2 = { nounwind readnone }
attributes #3 = { argmemonly nounwind }
attributes #4 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_rFOO: unknown scope"}
!4 = distinct !{!4, !"test_rFOO"}
!5 = !{!6}
!6 = distinct !{!6, !4, !"test_rFOO: rpFOO"}
!7 = !{!8}
!8 = distinct !{!8, !4, !"test_rFOO: tmp"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!11, i64 4, !"any pointer"}
!11 = !{!12, i64 1, !"omnipotent char"}
!12 = !{!"Simple C/C++ TBAA"}
!13 = !{!6, !8, !3}
!14 = !{!15, !16, !17}
!15 = !{i64 -1, i64 0}
!16 = !{i64 -1, i64 1}
!17 = !{i64 -1, i64 2}
!18 = !{!19, !10, i64 0, i64 4}
!19 = !{!11, i64 12, !"FOO", !10, i64 0, i64 4, !10, i64 4, i64 4, !10, i64 8, i64 4}
!20 = !{!19, !10, i64 4, i64 4}
!21 = !{!22, !22, i64 0, i64 4}
!22 = !{!11, i64 4, !"int"}
