; RUN: opt -S -loop-rotate < %s | FileCheck %s
; RUN: opt -S -loop-rotate -enable-mssa-loop-dependency=true -verify-memoryssa < %s | FileCheck %s
; RUN: opt -S -passes='require<targetir>,require<assumptions>,loop(loop-rotate)' < %s | FileCheck %s
; RUN: opt -S -passes='require<targetir>,require<assumptions>,loop(loop-rotate)' -enable-mssa-loop-dependency=true -verify-memoryssa  < %s | FileCheck %s

target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @g(i32*)

define void @test_02(i32* nocapture %_pA) nounwind ssp {
entry:
  %array = alloca [20 x i32], align 16
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  %cmp = icmp slt i32 %i.0, 100
  %arrayidx = getelementptr inbounds [20 x i32], [20 x i32]* %array, i64 0, i64 0
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  store i32 0, i32* %arrayidx, align 16
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %arrayidx.lcssa = phi i32* [ %arrayidx, %for.cond ]
  call void @g(i32* %arrayidx.lcssa) nounwind
  ret void
}

; CHECK-LABEL: @test_02(
; CHECK: entry:
; CHECK:   %p.decl1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
; CHECK:   %prov.p2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl1, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
; CHECK:    store i32 42, i32* %_pA, ptr_provenance i32* undef, align 16
; CHECK: for.body:
; CHECK:   %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !9)
; CHECK:   %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !11)
; CHECK:   %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !11), !tbaa !5, !noalias !11
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
; CHECK: for.end:


define void @test_03(i32* nocapture %_pA) nounwind ssp {
entry:
  %array = alloca [20 x i32], align 16
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp slt i32 %i.0, 100
  %arrayidx = getelementptr inbounds [20 x i32], [20 x i32]* %array, i64 0, i64 0
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  store i32 0, i32* %arrayidx, align 16
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %arrayidx.lcssa = phi i32* [ %arrayidx, %for.cond ]
  call void @g(i32* %arrayidx.lcssa) nounwind
  ret void
}
; CHECK-LABEL: @test_03(
; CHECK: entry:
; CHECK: for.body:
; CHECK:   %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !9)
; CHECK:   %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !9), !tbaa !5, !noalias !9
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
; CHECK: for.end:

define void @test_04(i32* nocapture %_pA) nounwind ssp {
entry:
  %array = alloca [20 x i32], align 16
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  %cmp = icmp slt i32 %i.0, 100
  %arrayidx = getelementptr inbounds [20 x i32], [20 x i32]* %array, i64 0, i64 0
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  store i32 0, i32* %arrayidx, align 16
  store i32 43, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %arrayidx.lcssa = phi i32* [ %arrayidx, %for.cond ]
  call void @g(i32* %arrayidx.lcssa) nounwind
  ret void
}
; CHECK-LABEL: @test_04(
; CHECK: entry:
; CHECK:   %p.decl1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !13)
; CHECK:   %prov.p2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl1, i32** null, i32** undef, i32 0, metadata !13), !tbaa !5, !noalias !13
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* undef, align 16
; CHECK: for.body:
; CHECK:   %prov.p4 = phi i32* [ %prov.p2, %entry ], [ %prov.p, %for.body ]
; CHECK:   %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !9)
; CHECK:   store i32 43, i32* %_pA, ptr_provenance i32* %prov.p4, align 16
; CHECK:   %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !15)
; CHECK:   %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !15), !tbaa !5, !noalias !15
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
; CHECK: for.end:

define void @test_05(i32* nocapture %_pA) nounwind ssp {
entry:
  %array = alloca [20 x i32], align 16
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  %cmp = icmp slt i32 %i.0, 100
  %arrayidx = getelementptr inbounds [20 x i32], [20 x i32]* %array, i64 0, i64 0
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  store i32 0, i32* %arrayidx, align 16
  store i32 43, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %arrayidx.lcssa = phi i32* [ %arrayidx, %for.cond ]
  store i32 44, i32* %_pA, ptr_provenance i32* %prov.p, align 16
  call void @g(i32* %arrayidx.lcssa) nounwind
  ret void
}
; CHECK-LABEL: @test_05(
; CHECK: entry:
; CHECK:   %p.decl1 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !17)
; CHECK:   %prov.p2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %p.decl1, i32** null, i32** undef, i32 0, metadata !17), !tbaa !5, !noalias !17
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* undef, align 16
; CHECK: for.body:
; CHECK:   %prov.p4 = phi i32* [ %prov.p2, %entry ], [ %prov.p, %for.body ]
; CHECK:   %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !9)
; CHECK:   store i32 43, i32* %_pA, ptr_provenance i32* %prov.p4, align 16
; CHECK:   %0 = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !19)
; CHECK:   %prov.p = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_pA, i8* %0, i32** null, i32** undef, i32 0, metadata !19), !tbaa !5, !noalias !19
; CHECK:   store i32 42, i32* %_pA, ptr_provenance i32* %prov.p, align 16
; CHECK: for.end:
; CHECK:   %prov.p.lcssa = phi i32* [ %prov.p, %for.body ]
; CHECK:   store i32 44, i32* %_pA, ptr_provenance i32* %prov.p.lcssa, align 16


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
!3 = distinct !{!3, !4, !"test_loop_rotate_XX: pA"}
!4 = distinct !{!4, !"test_loop_rotate_XX"}
!5 = !{!6, !6, i64 0, i64 4}
!6 = !{!7, i64 4, !"any pointer"}
!7 = !{!8, i64 1, !"omnipotent char"}
!8 = !{!"Simple C/C++ TBAA"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!7, i64 4, !"int"}
