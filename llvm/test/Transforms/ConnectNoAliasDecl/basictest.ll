; RUN: opt < %s -connect-noaliasdecl -verify -S | FileCheck %s
; RUN: opt < %s -passes=connect-noaliasdecl,verify -S | FileCheck %s

target datalayout = "e-p:64:64:64-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-n8:16:32:64"


%struct.FOO = type { i32*, i32*, i32* }

; Function Attrs: nounwind
define dso_local void @test_01_before(i32** %_p, i32 %c) #0 !noalias !2 {
entry:
  %rp = alloca [2 x i32*], align 4
  %other = alloca i32*, align 4
  %local_tmp = alloca i32*, align 4
  %tmp.0 = bitcast [2 x i32*]* %rp to i8*
  %.fca.0.gep = getelementptr inbounds [2 x i32*], [2 x i32*]* %rp, i32 0, i32 0
  %.fca.1.gep = getelementptr inbounds [2 x i32*], [2 x i32*]* %rp, i32 0, i32 1
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %tmp.0) #5, !noalias !5
  %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0a2p0i32.i32([2 x i32*]* %rp, i32 0, metadata !7)
  %tmp.2 = load i32*, i32** %_p, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.2, i32** %.fca.0.gep, align 4, !tbaa !8, !noalias !5
  %arrayinit.element = getelementptr inbounds i32*, i32** %.fca.0.gep, i32 1
  %arrayidx1 = getelementptr inbounds i32*, i32** %_p, i32 1
  %tmp.3 = load i32*, i32** %arrayidx1, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.3, i32** %arrayinit.element, align 4, !tbaa !8, !noalias !5
  %tmp.4 = bitcast i32** %other to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %tmp.4) #5, !noalias !5
  %arrayidx2 = getelementptr inbounds i32*, i32** %_p, i32 2
  %tmp.5 = load i32*, i32** %arrayidx2, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.5, i32** %other, align 4, !tbaa !8, !noalias !5
  %tobool = icmp ne i32 %c, 0
  %cond = select i1 %tobool, i32** %.fca.0.gep, i32** %other
  %tmp.6 = load i32*, i32** %arrayinit.element, align 4, !tbaa !8, !noalias !5
  %through_local_tmp = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.6, i8* null, i32** %arrayinit.element, i32 0, metadata !2), !tbaa !8, !noalias !5
  %tmp.7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %through_local_tmp, i8* null, i32** %local_tmp, i32 0, metadata !2), !tbaa !8, !noalias !5
  %tmp.8 = load i32, i32* %tmp.7, align 4, !tbaa !12, !noalias !5
  %tmp.9 = load i32*, i32** %.fca.0.gep, align 4, !tbaa !8, !noalias !5
  %tmp.10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.9, i8* null, i32** %.fca.0.gep, i32 0, metadata !2), !tbaa !8, !noalias !5
  store i32 %tmp.8, i32* %tmp.10, align 4, !tbaa !12, !noalias !5
  %tmp.11 = load i32*, i32** %cond, align 4, !tbaa !8, !noalias !5
  %tmp.12 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.11, i8* null, i32** %cond, i32 0, metadata !2), !tbaa !8, !noalias !5
  store i32 42, i32* %tmp.12, align 4, !tbaa !12, !noalias !5
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %tmp.4) #5
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %tmp.0) #5
  ret void
}

; CHECK-LABEL: @test_01_before(
; CHECK: %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0a2p0i32.i32([2 x i32*]* %rp, i32 0, metadata !7)
; CHECK: %through_local_tmp = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.6, i8* %tmp.1, i32** %arrayinit.element, i32 0, metadata !7), !tbaa !8, !noalias !5
; CHECK: %tmp.10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.9, i8* %tmp.1, i32** %.fca.0.gep, i32 0, metadata !7), !tbaa !8, !noalias !5
; CHECK: %tmp.12 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %tmp.11, i8* null, i32** %cond, i32 0, metadata !2), !tbaa !8, !noalias !5
; CHECK-NOT: llvm.noalias

; Function Attrs: nounwind
define dso_local void @test_02(i32** %_p, i32 %c) #0 !noalias !14 {
entry:
  %foo = alloca %struct.FOO, align 4
  %tmp = alloca %struct.FOO, align 4
  %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i32(%struct.FOO* %foo, i32 0, metadata !17)
  %tmp.10 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i32(%struct.FOO* %tmp, i32 0, metadata !19)
  %tmp.12 = call %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO* %foo, i8* null, metadata !21, metadata !14)
  %tmp.13 = load %struct.FOO, %struct.FOO* %tmp.12, align 4, !noalias !25
  store %struct.FOO %tmp.13, %struct.FOO* %tmp, !noalias !25
  ret void
}

; CHECK-LABEL: @test_02(
; CHECK: %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i32(%struct.FOO* %foo, i32 0, metadata !17)
; CHECK: %tmp.10 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i32(%struct.FOO* %tmp, i32 0, metadata !19)
; CHECK: %tmp.12 = call %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO* %foo, i8* %tmp.1, metadata !21, metadata !17)
; CHECK-NOT: llvm.noalias

; Function Attrs: nounwind
define dso_local void @test_01_after(i32** %_p, i32 %c) #0 !noalias !2 {
entry:
  %rp = alloca [2 x i32*], align 4
  %local_tmp = alloca i32*, align 4
  %other = alloca i32*, align 4
  %.fca.0.gep = getelementptr inbounds [2 x i32*], [2 x i32*]* %rp, i32 0, i32 0
  %.fca.1.gep = getelementptr inbounds [2 x i32*], [2 x i32*]* %rp, i32 0, i32 1
  %tmp.0 = bitcast [2 x i32*]* %rp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %tmp.0) #5, !noalias !5
  %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0a2p0i32.i32([2 x i32*]* %rp, i32 0, metadata !7)
  %tmp.2 = load i32*, i32** %_p, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.2, i32** %.fca.0.gep, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %arrayinit.element = getelementptr inbounds i32*, i32** %.fca.0.gep, i32 1
  %arrayidx1 = getelementptr inbounds i32*, i32** %_p, i32 1
  %tmp.3 = load i32*, i32** %arrayidx1, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.3, i32** %arrayinit.element, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %tmp.4 = bitcast i32** %other to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %tmp.4) #5, !noalias !5
  %arrayidx2 = getelementptr inbounds i32*, i32** %_p, i32 2
  %tmp.5 = load i32*, i32** %arrayidx2, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  store i32* %tmp.5, i32** %other, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %tobool = icmp ne i32 %c, 0
  %cond = select i1 %tobool, i32** %.fca.0.gep, i32** %other
  %tmp.6 = load i32*, i32** %arrayinit.element, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %through_local_tmp = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.6, i8* null, i32** %arrayinit.element, i32** undef, i32 0, metadata !2), !tbaa !8, !noalias !5
  %tmp.7 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %through_local_tmp, i8* null, i32** %local_tmp, i32** undef, i32 0, metadata !2), !tbaa !8, !noalias !5
  %tmp.8 = load i32, i32* %tmp.6, ptr_provenance i32* %tmp.7, align 4, !tbaa !12, !noalias !5
  %tmp.9 = load i32*, i32** %.fca.0.gep, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %tmp.10 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.9, i8* null, i32** %.fca.0.gep, i32** undef, i32 0, metadata !2), !tbaa !8, !noalias !5
  store i32 %tmp.8, i32* %tmp.9, ptr_provenance i32* %tmp.10, align 4, !tbaa !12, !noalias !5
  %tmp.11 = load i32*, i32** %cond, ptr_provenance i32** undef, align 4, !tbaa !8, !noalias !5
  %tmp.12 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.11, i8* null, i32** %cond, i32** undef, i32 0, metadata !2), !tbaa !8, !noalias !5
  store i32 42, i32* %tmp.11, ptr_provenance i32* %tmp.12, align 4, !tbaa !12, !noalias !5
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %tmp.4) #5
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %tmp.0) #5
  ret void
}

; CHECK-LABEL: @test_01_after(
; CHECK: %tmp.1 = call i8* @llvm.noalias.decl.p0i8.p0a2p0i32.i32([2 x i32*]* %rp, i32 0, metadata !7)
; CHECK: %through_local_tmp = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.6, i8* %tmp.1, i32** %arrayinit.element, i32** undef, i32 0, metadata !7), !tbaa !8, !noalias !5
; CHECK: %tmp.10 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.9, i8* %tmp.1, i32** %.fca.0.gep, i32** undef, i32 0, metadata !7), !tbaa !8, !noalias !5
; CHECK: %tmp.12 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %tmp.11, i8* null, i32** %cond, i32** undef, i32 0, metadata !2), !tbaa !8, !noalias !5
; CHECK-NOT: llvm.noalias

; CHECK: declare

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0a2p0i32.i32([2 x i32*]*, i32, metadata) #1

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) #2

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i32(%struct.FOO*, i32, metadata) #1

; Function Attrs: nounwind readnone
declare %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO*, i8*, metadata, metadata) #3

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #4


attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { argmemonly nounwind speculatable }
attributes #3 = { nounwind readnone }
attributes #4 = { nounwind readnone speculatable }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"test_01: unknown scope"}
!4 = distinct !{!4, !"test_01"}
!5 = !{!6, !3}
!6 = distinct !{!6, !4, !"test_01: rp"}
!7 = !{!6}
!8 = !{!9, !9, i64 0, i64 4}
!9 = !{!10, i64 4, !"any pointer"}
!10 = !{!11, i64 1, !"omnipotent char"}
!11 = !{!"Simple C/C++ TBAA"}
!12 = !{!13, !13, i64 0, i64 4}
!13 = !{!10, i64 4, !"int"}
!14 = !{!15}
!15 = distinct !{!15, !16, !"test_02: unknown scope"}
!16 = distinct !{!16, !"test_02"}
!17 = !{!18}
!18 = distinct !{!18, !16, !"test_02: foo"}
!19 = !{!20}
!20 = distinct !{!20, !16, !"test_02: tmp"}
!21 = !{!22, !23, !24}
!22 = !{i64 -1, i64 0}
!23 = !{i64 -1, i64 1}
!24 = !{i64 -1, i64 2}
!25 = !{!20, !18, !15}
