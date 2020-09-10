; RUN: opt < %s -sroa -S | FileCheck %s
; RUN: opt < %s -passes=sroa -S | FileCheck %s

target datalayout = "e-p:64:64:64-p1:16:16:16-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-n8:16:32:64"


%struct.FOO = type { i32*, i32*, i32* }

; Function Attrs: nounwind
define dso_local void @test_ri(i32* %_p) #0 {
entry:
  %_p.addr = alloca i32*, align 4
  %rp = alloca i32*, align 4
  store i32* %_p, i32** %_p.addr, align 4, !tbaa !2, !noalias !6
  store i32* undef, i32** %rp, align 4, !noalias !6
  %0 = bitcast i32** %rp to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #4, !noalias !6
  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** %rp, i64 0, metadata !6)
  %2 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !6
  store i32* %2, i32** %rp, align 4, !tbaa !2, !noalias !6
  %3 = load i32*, i32** %rp, align 4, !tbaa !2, !noalias !6
  %4 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %3, i8* %1, i32** %rp, i64 0, metadata !6), !tbaa !2, !noalias !6
  store i32 42, i32* %4, align 4, !tbaa !9, !noalias !6
  %5 = bitcast i32** %rp to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %5) #4
  ret void
}

; CHECK-LABEL: @test_ri(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !2)
; CHECK:  %1 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i64 0, metadata !2), !tbaa !5, !noalias !2

; Function Attrs: nounwind
define dso_local void @test_ra(i32* %_p) #0 {
entry:
  %_p.addr = alloca i32*, align 4
  %rp = alloca [3 x i32*], align 4
  store i32* %_p, i32** %_p.addr, align 4, !tbaa !2, !noalias !11
  store [3 x i32*] undef, [3 x i32*]* %rp, align 4, !noalias !11
  %0 = bitcast [3 x i32*]* %rp to i8*
  call void @llvm.lifetime.start.p0i8(i64 12, i8* %0) #4, !noalias !11
  %1 = call i8* @llvm.noalias.decl.p0i8.p0a3p0i32.i64([3 x i32*]* %rp, i64 0, metadata !11)
  %arrayinit.begin = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 0
  %2 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !11
  store i32* %2, i32** %arrayinit.begin, align 4, !tbaa !2, !noalias !11
  %arrayinit.element = getelementptr inbounds i32*, i32** %arrayinit.begin, i32 1
  %3 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !11
  %add.ptr = getelementptr inbounds i32, i32* %3, i32 1
  store i32* %add.ptr, i32** %arrayinit.element, align 4, !tbaa !2, !noalias !11
  %arrayinit.element1 = getelementptr inbounds i32*, i32** %arrayinit.element, i32 1
  %4 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !11
  %add.ptr2 = getelementptr inbounds i32, i32* %4, i32 2
  store i32* %add.ptr2, i32** %arrayinit.element1, align 4, !tbaa !2, !noalias !11
  %arrayidx = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 0
  %5 = load i32*, i32** %arrayidx, align 4, !tbaa !2, !noalias !11
  %6 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %5, i8* %1, i32** %arrayidx, i64 0, metadata !11), !tbaa !2, !noalias !11
  store i32 42, i32* %6, align 4, !tbaa !9, !noalias !11
  %arrayidx3 = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 1
  %7 = load i32*, i32** %arrayidx3, align 4, !tbaa !2, !noalias !11
  %8 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %7, i8* %1, i32** %arrayidx3, i64 0, metadata !11), !tbaa !2, !noalias !11
  store i32 43, i32* %8, align 4, !tbaa !9, !noalias !11
  %arrayidx4 = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 2
  %9 = load i32*, i32** %arrayidx4, align 4, !tbaa !2, !noalias !11
  %10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %9, i8* %1, i32** %arrayidx4, i64 0, metadata !11), !tbaa !2, !noalias !11
  store i32 44, i32* %10, align 4, !tbaa !9, !noalias !11
  %11 = bitcast [3 x i32*]* %rp to i8*
  call void @llvm.lifetime.end.p0i8(i64 12, i8* %11) #4
  ret void
}

; CHECK-LABEL: @test_ra(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !11)
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 8, metadata !11)
; CHECK:  %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 16, metadata !11)
; CHECK:  %3 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i64 0, metadata !11), !tbaa !5, !noalias !11
; CHECK:  %4 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %add.ptr, i8* %1, i32** null, i64 8, metadata !11), !tbaa !5, !noalias !11
; CHECK:  %5 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %add.ptr2, i8* %2, i32** null, i64 16, metadata !11), !tbaa !5, !noalias !11

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0a3p0i32.i64([3 x i32*]*, i64, metadata) #1

; Function Attrs: nounwind
define dso_local void @test_rs(i32* %_p) #0 {
entry:
  %_p.addr = alloca i32*, align 4
  %foo = alloca %struct.FOO, align 4
  store i32* %_p, i32** %_p.addr, align 4, !tbaa !2, !noalias !14
  store %struct.FOO undef, %struct.FOO* %foo, align 4, !noalias !14
  %0 = bitcast %struct.FOO* %foo to i8*
  call void @llvm.lifetime.start.p0i8(i64 12, i8* %0) #4, !noalias !14
  %1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i64(%struct.FOO* %foo, i64 0, metadata !14)
  %mP0 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 0
  %2 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !14
  store i32* %2, i32** %mP0, align 4, !tbaa !17, !noalias !14
  %mP1 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 1
  %3 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !14
  %add.ptr = getelementptr inbounds i32, i32* %3, i32 1
  store i32* %add.ptr, i32** %mP1, align 4, !tbaa !19, !noalias !14
  %mP2 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 2
  %4 = load i32*, i32** %_p.addr, align 4, !tbaa !2, !noalias !14
  %add.ptr1 = getelementptr inbounds i32, i32* %4, i32 2
  store i32* %add.ptr1, i32** %mP2, align 4, !tbaa !20, !noalias !14
  %mP02 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 0
  %5 = load i32*, i32** %mP02, align 4, !tbaa !17, !noalias !14
  %6 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %5, i8* %1, i32** %mP02, i64 0, metadata !14), !tbaa !17, !noalias !14
  store i32 42, i32* %6, align 4, !tbaa !9, !noalias !14
  %mP13 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 1
  %7 = load i32*, i32** %mP13, align 4, !tbaa !19, !noalias !14
  %8 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %7, i8* %1, i32** %mP13, i64 0, metadata !14), !tbaa !19, !noalias !14
  store i32 43, i32* %8, align 4, !tbaa !9, !noalias !14
  %mP24 = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 2
  %9 = load i32*, i32** %mP24, align 4, !tbaa !20, !noalias !14
  %10 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %9, i8* %1, i32** %mP24, i64 0, metadata !14), !tbaa !20, !noalias !14
  store i32 44, i32* %10, align 4, !tbaa !9, !noalias !14
  %11 = bitcast %struct.FOO* %foo to i8*
  call void @llvm.lifetime.end.p0i8(i64 12, i8* %11) #4
  ret void
}

; CHECK-LABEL: @test_rs(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !14)
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 8, metadata !14)
; CHECK:  %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 16, metadata !14)
; CHECK:  %3 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i64 0, metadata !14), !tbaa !17, !noalias !14
; CHECK:  %4 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %add.ptr, i8* %1, i32** null, i64 8, metadata !14), !tbaa !19, !noalias !14
; CHECK:  %5 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %add.ptr1, i8* %2, i32** null, i64 16, metadata !14), !tbaa !20, !noalias !14

; Function Attrs: argmemonly nounwind speculatable
define dso_local void @test_ri_inlined(i32* %_p) local_unnamed_addr #2 !noalias !21 {
entry:
  %rp = alloca i32*, align 4
  %0 = bitcast i32** %rp to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #4, !noalias !24
  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** nonnull %rp, i64 0, metadata !27), !noalias !21
  store i32* %_p, i32** %rp, ptr_provenance i32** undef, align 4, !noalias !24
  %2 = load i32*, i32** %rp, ptr_provenance i32** undef, align 4, !noalias !28
  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* %1, i32** %rp, i32** undef, i64 0, metadata !27) #4, !noalias !28
  store i32 42, i32* %2, ptr_provenance i32* %3, align 4, !noalias !28
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #4, !noalias !21
  ret void
}

; CHECK-LABEL: @test_ri_inlined(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !24)
; CHECK:  %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i32** undef, i64 0, metadata !24){{.*}}, !noalias !27

; Function Attrs: argmemonly nounwind speculatable
define dso_local void @test_ra_inlined(i32* %_p) local_unnamed_addr #2 !noalias !29 {
entry:
  %rp = alloca [3 x i32*], align 4
  %.fca.0.gep = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 0
  %0 = bitcast [3 x i32*]* %rp to i8*
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %0) #4, !noalias !32
  %1 = call i8* @llvm.noalias.decl.p0i8.p0a3p0i32.i64([3 x i32*]* nonnull %rp, i64 0, metadata !35), !noalias !29
  store i32* %_p, i32** %.fca.0.gep, ptr_provenance i32** undef, align 4, !noalias !32
  %arrayinit.element = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 1
  %add.ptr = getelementptr inbounds i32, i32* %_p, i32 1
  store i32* %add.ptr, i32** %arrayinit.element, ptr_provenance i32** undef, align 4, !noalias !32
  %arrayinit.element1 = getelementptr inbounds [3 x i32*], [3 x i32*]* %rp, i32 0, i32 2
  %add.ptr2 = getelementptr inbounds i32, i32* %_p, i32 2
  store i32* %add.ptr2, i32** %arrayinit.element1, ptr_provenance i32** undef, align 4, !noalias !32
  %2 = load i32*, i32** %.fca.0.gep, ptr_provenance i32** undef, align 4, !noalias !36
  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* %1, i32** %.fca.0.gep, i32** undef, i64 0, metadata !35) #4, !noalias !36
  store i32 42, i32* %2, ptr_provenance i32* %3, align 4, !noalias !36
  %arrayidx1.i = getelementptr inbounds i32*, i32** %.fca.0.gep, i32 1
  %4 = load i32*, i32** %arrayidx1.i, ptr_provenance i32** undef, align 4, !noalias !36
  %5 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %4, i8* %1, i32** nonnull %arrayidx1.i, i32** undef, i64 0, metadata !35) #4, !noalias !36
  store i32 43, i32* %4, ptr_provenance i32* %5, align 4, !noalias !36
  %arrayidx2.i = getelementptr inbounds i32*, i32** %.fca.0.gep, i32 2
  %6 = load i32*, i32** %arrayidx2.i, ptr_provenance i32** undef, align 4, !noalias !36
  %7 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %6, i8* %1, i32** nonnull %arrayidx2.i, i32** undef, i64 0, metadata !35) #4, !noalias !36
  store i32 44, i32* %6, ptr_provenance i32* %7, align 4, !noalias !36
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %0) #4, !noalias !29
  ret void
}

; CHECK-LABEL: @test_ra_inlined(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !31)
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 8, metadata !31)
; CHECK:  %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 16, metadata !31)
; CHECK:  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i32** undef, i64 0, metadata !31){{.*}}, !noalias !34
; CHECK:  %4 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %add.ptr, i8* %1, i32** nonnull null, i32** undef, i64 8, metadata !31){{.*}}, !noalias !34
; CHECK:  %5 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %add.ptr2, i8* %2, i32** nonnull null, i32** undef, i64 16, metadata !31){{.*}}, !noalias !34


; Function Attrs: argmemonly nounwind speculatable
define dso_local void @test_rs_inlined(i32* %_p) local_unnamed_addr #2 !noalias !37 {
entry:
  %foo = alloca %struct.FOO, align 4
  %.fca.0.gep = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 0
  %.fca.1.gep = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 1
  %.fca.2.gep = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 2
  %0 = bitcast %struct.FOO* %foo to i8*
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %0) #4, !noalias !40
  %1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i64(%struct.FOO* nonnull %foo, i64 0, metadata !43), !noalias !37
  store i32* %_p, i32** %.fca.0.gep, ptr_provenance i32** undef, align 4, !noalias !40
  %add.ptr = getelementptr inbounds i32, i32* %_p, i32 1
  store i32* %add.ptr, i32** %.fca.1.gep, ptr_provenance i32** undef, align 4, !noalias !40
  %add.ptr1 = getelementptr inbounds i32, i32* %_p, i32 2
  store i32* %add.ptr1, i32** %.fca.2.gep, ptr_provenance i32** undef, align 4, !noalias !40
  %mP0.i = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 0
  %2 = load i32*, i32** %mP0.i, ptr_provenance i32** undef, align 4, !noalias !44
  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %2, i8* %1, i32** %mP0.i, i32** undef, i64 0, metadata !43) #4, !noalias !44
  store i32 42, i32* %2, ptr_provenance i32* %3, align 4, !noalias !44
  %mP1.i = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 1
  %4 = load i32*, i32** %mP1.i, ptr_provenance i32** undef, align 4, !noalias !44
  %5 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %4, i8* %1, i32** nonnull %mP1.i, i32** undef, i64 0, metadata !43) #4, !noalias !44
  store i32 43, i32* %4, ptr_provenance i32* %5, align 4, !noalias !44
  %mP2.i = getelementptr inbounds %struct.FOO, %struct.FOO* %foo, i32 0, i32 2
  %6 = load i32*, i32** %mP2.i, ptr_provenance i32** undef, align 4, !noalias !44
  %7 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %6, i8* %1, i32** nonnull %mP2.i, i32** undef, i64 0, metadata !43) #4, !noalias !44
  store i32 44, i32* %6, ptr_provenance i32* %7, align 4, !noalias !44
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %0) #4, !noalias !37
  ret void
}

; CHECK-LABEL: @test_rs_inlined(
; CHECK-NOT: alloca
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !38)
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 8, metadata !38)
; CHECK:  %2 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 16, metadata !38)
; CHECK:  %3 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %_p, i8* %0, i32** null, i32** undef, i64 0, metadata !38){{.*}}, !noalias !41
; CHECK:  %4 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %add.ptr, i8* %1, i32** nonnull null, i32** undef, i64 8, metadata !38){{.*}}, !noalias !41
; CHECK:  %5 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %add.ptr1, i8* %2, i32** nonnull null, i32** undef, i64 16, metadata !38){{.*}}, !noalias !41

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i64(%struct.FOO*, i64, metadata) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32**, i64, metadata) #1

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32*, i8*, i32**, i64, metadata) #2

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32*, i8*, i32**, i32**, i64, metadata) #3

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { argmemonly nounwind speculatable }
attributes #3 = { nounwind readnone speculatable }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3, !3, i64 0, i64 4}
!3 = !{!4, i64 4, !"any pointer"}
!4 = !{!5, i64 1, !"omnipotent char"}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7}
!7 = distinct !{!7, !8, !"test_ri: rp"}
!8 = distinct !{!8, !"test_ri"}
!9 = !{!10, !10, i64 0, i64 4}
!10 = !{!4, i64 4, !"int"}
!11 = !{!12}
!12 = distinct !{!12, !13, !"test_ra: rp"}
!13 = distinct !{!13, !"test_ra"}
!14 = !{!15}
!15 = distinct !{!15, !16, !"test_rs: foo"}
!16 = distinct !{!16, !"test_rs"}
!17 = !{!18, !3, i64 0, i64 4}
!18 = !{!4, i64 12, !"FOO", !3, i64 0, i64 4, !3, i64 4, i64 4, !3, i64 8, i64 4}
!19 = !{!18, !3, i64 4, i64 4}
!20 = !{!18, !3, i64 8, i64 4}
!21 = !{!22}
!22 = distinct !{!22, !23, !"test_ri_inlined: unknown scope"}
!23 = distinct !{!23, !"test_ri_inlined"}
!24 = !{!25, !22}
!25 = distinct !{!25, !26, !"test_ri_inlined: rp"}
!26 = distinct !{!26, !"test_ri_inlined"}
!27 = !{!25}
!28 = !{!22, !25, !22}
!29 = !{!30}
!30 = distinct !{!30, !31, !"test_ra_inlined: unknown scope"}
!31 = distinct !{!31, !"test_ra_inlined"}
!32 = !{!33, !30}
!33 = distinct !{!33, !34, !"test_ra_inlined: rp"}
!34 = distinct !{!34, !"test_ra_inlined"}
!35 = !{!33}
!36 = !{!30, !33, !30}
!37 = !{!38}
!38 = distinct !{!38, !39, !"test_rs_inlined: unknown scope"}
!39 = distinct !{!39, !"test_rs_inlined"}
!40 = !{!41, !38}
!41 = distinct !{!41, !42, !"test_rs_inlined: foo"}
!42 = distinct !{!42, !"test_rs_inlined"}
!43 = !{!41}
!44 = !{!38, !41, !38}
