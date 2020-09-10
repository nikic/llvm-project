; using memcpy:
; RUN: sed < %s -e 's,;V1 ,    ,' | opt -sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V1
; RUN: sed < %s -e 's,;V1 ,    ,' | opt -passes=sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V1

; using aggregate load/store:
; RUN: sed < %s -e 's,;V2 ,    ,' | opt -sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V2
; RUN: sed < %s -e 's,;V2 ,    ,' | opt -passes=sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V2

; using i64 load/store:
; RUN: sed < %s -e 's,;V3 ,    ,' | opt -sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V3
; RUN: sed < %s -e 's,;V3 ,    ,' | opt -passes=sroa -S | FileCheck %s  --check-prefixes=CHECK,CHECK_V3


; Validate that SROA correctly deduces noalias pointers when removing:
; - llvm.mempcy
; - aggregate load/store
; - copying the struct through i64

; General form of each function is based on:
; ------
; struct FOO {
;   int* __restrict p;
; };
; 
; struct FUM {
;   int* __restrict p0;
;   struct FOO m1;
; };
; 
; void test01(struct FUM* a_fum)
; {
;   struct FUM l_fum = *a_fum;
;   *l_fum.p0 = 42;
; }
; 
; void test02(struct FUM* a_fum)
; {
;   struct FUM l_fum = *a_fum;
;   *l_fum.m1.p = 43;
; }
; 
; void test03(struct FUM* a_fum)
; {
;   struct FUM l_fum = *a_fum;
;   *l_fum.p0 = 42;
;   *l_fum.m1.p = 43;
; }


; ModuleID = 'test3.c'
source_filename = "test3.c"
target datalayout = "e-m:e-p:32:32-p270:32:32-p271:32:32-p272:64:64-f64:32:64-f80:32-n8:16:32-S128"
target triple = "i386-unknown-linux-gnu"

%struct.FUM = type { i32*, %struct.FOO }
%struct.FOO = type { i32* }

; Function Attrs: nounwind
define dso_local void @test01(%struct.FUM* %a_fum) #0 !noalias !3 {
entry:
  %a_fum.addr = alloca %struct.FUM*, align 4
  %l_fum = alloca %struct.FUM, align 4
  store %struct.FUM* %a_fum, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !10
  %tmp0 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %tmp0) #5, !noalias !10
  %tmp1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FUMs.i64(%struct.FUM* %l_fum, i64 0, metadata !12), !noalias !10
  %tmp2 = load %struct.FUM*, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !10
  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %tmp2, i8* null, metadata !13, metadata !3)

;V1  %tmp4 = bitcast %struct.FUM* %l_fum to i8*
;V1  %tmp5 = bitcast %struct.FUM* %tmp3 to i8*
;V1  call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %tmp4, i8* align 4 %tmp5, i32 8, i1 false), !tbaa.struct !16, !noalias !10

;V2  %cp1 = load %struct.FUM, %struct.FUM* %tmp3, align 4
;V2  store %struct.FUM %cp1, %struct.FUM* %l_fum, align 4

;V3  %tmp4 = bitcast %struct.FUM* %l_fum to i64*
;V3  %tmp5 = bitcast %struct.FUM* %tmp3 to i64*
;V3  %cp1 = load i64, i64* %tmp5, align 4
;V3  store i64 %cp1, i64* %tmp4, align 4

  %p0 = getelementptr inbounds %struct.FUM, %struct.FUM* %l_fum, i32 0, i32 0
  %tmp6 = load i32*, i32** %p0, align 4, !tbaa !17, !noalias !10
  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %tmp6, i8* %tmp1, i32** %p0, i64 0, metadata !12), !tbaa !17, !noalias !10
  store i32 42, i32* %tmp7, align 4, !tbaa !20, !noalias !10
  %tmp8 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %tmp8) #5
  ret void
}

; CHECK-LABEL: test01
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata !6)
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 4, metadata !6)
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK_V2:  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %a_fum, i8* null, metadata !8, metadata !3)
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata !3)
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata !3)
; CHECK:  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* %0, i32** null, i64 0, metadata !6)
; CHECK: ret void

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg %tmp0, i8* nocapture %tmp1) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0s_struct.FUMs.i64(%struct.FUM* %tmp0, i64 %tmp1, metadata %tmp2) #2

; Function Attrs: nounwind readnone
declare %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %tmp0, i8* %tmp1, metadata %tmp2, metadata %tmp3) #3

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* noalias nocapture writeonly %tmp0, i8* noalias nocapture readonly %tmp1, i32 %tmp2, i1 immarg %tmp3) #1

; Function Attrs: argmemonly nounwind speculatable
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %tmp0, i8* %tmp1, i32** %tmp2, i64 %tmp3, metadata %tmp4) #4

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg %tmp0, i8* nocapture %tmp1) #1

; Function Attrs: nounwind
define dso_local void @test02(%struct.FUM* %a_fum) #0 !noalias !22 {
entry:
  %a_fum.addr = alloca %struct.FUM*, align 4
  %l_fum = alloca %struct.FUM, align 4
  store %struct.FUM* %a_fum, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !25
  %tmp0 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %tmp0) #5, !noalias !25
  %tmp1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FUMs.i64(%struct.FUM* %l_fum, i64 0, metadata !27), !noalias !25
  %tmp2 = load %struct.FUM*, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !25
  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %tmp2, i8* null, metadata !13, metadata !22)

;V1  %tmp4 = bitcast %struct.FUM* %l_fum to i8*
;V1  %tmp5 = bitcast %struct.FUM* %tmp3 to i8*
;V1  call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %tmp4, i8* align 4 %tmp5, i32 8, i1 false), !tbaa.struct !16, !noalias !25

;V2  %cp1 = load %struct.FUM, %struct.FUM* %tmp3, align 4
;V2  store %struct.FUM %cp1, %struct.FUM* %l_fum, align 4

;V3  %tmp4 = bitcast %struct.FUM* %l_fum to i64*
;V3  %tmp5 = bitcast %struct.FUM* %tmp3 to i64*
;V3  %cp1 = load i64, i64* %tmp5, align 4
;V3  store i64 %cp1, i64* %tmp4, align 4

  %m1 = getelementptr inbounds %struct.FUM, %struct.FUM* %l_fum, i32 0, i32 1
  %p = getelementptr inbounds %struct.FOO, %struct.FOO* %m1, i32 0, i32 0
  %tmp6 = load i32*, i32** %p, align 4, !tbaa !28, !noalias !25
  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %tmp6, i8* %tmp1, i32** %p, i64 0, metadata !27), !tbaa !28, !noalias !25
  store i32 43, i32* %tmp7, align 4, !tbaa !20, !noalias !25
  %tmp8 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %tmp8) #5
  ret void
}

; CHECK-LABEL: test02
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata ![[SCOPE2:[0-9]+]])
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 4, metadata ![[SCOPE2]])
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK_V2:  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %a_fum, i8* null, metadata !{{[0-9]+}}, metadata !{{[0-9]+}})
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata ![[SCOPE2_OUT:[0-9]+]])
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata ![[SCOPE2_OUT]])
; CHECK:  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* %1, i32** null, i64 4, metadata ![[SCOPE2]])
; CHECK: ret void

; Function Attrs: nounwind
define dso_local void @test03(%struct.FUM* %a_fum) #0 !noalias !29 {
entry:
  %a_fum.addr = alloca %struct.FUM*, align 4
  %l_fum = alloca %struct.FUM, align 4
  store %struct.FUM* %a_fum, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !32
  %tmp0 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %tmp0) #5, !noalias !32
  %tmp1 = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FUMs.i64(%struct.FUM* %l_fum, i64 0, metadata !34), !noalias !32
  %tmp2 = load %struct.FUM*, %struct.FUM** %a_fum.addr, align 4, !tbaa !6, !noalias !32
  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %tmp2, i8* null, metadata !13, metadata !29)

;V1  %tmp4 = bitcast %struct.FUM* %l_fum to i8*
;V1  %tmp5 = bitcast %struct.FUM* %tmp3 to i8*
;V1  call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %tmp4, i8* align 4 %tmp5, i32 8, i1 false), !tbaa.struct !16, !noalias !32

;V2  %cp1 = load %struct.FUM, %struct.FUM* %tmp3, align 4
;V2  store %struct.FUM %cp1, %struct.FUM* %l_fum, align 4

;V3  %tmp4 = bitcast %struct.FUM* %l_fum to i64*
;V3  %tmp5 = bitcast %struct.FUM* %tmp3 to i64*
;V3  %cp1 = load i64, i64* %tmp5, align 4
;V3  store i64 %cp1, i64* %tmp4, align 4

  %p0 = getelementptr inbounds %struct.FUM, %struct.FUM* %l_fum, i32 0, i32 0
  %tmp6 = load i32*, i32** %p0, align 4, !tbaa !17, !noalias !32
  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %tmp6, i8* %tmp1, i32** %p0, i64 0, metadata !34), !tbaa !17, !noalias !32
  store i32 42, i32* %tmp7, align 4, !tbaa !20, !noalias !32
  %m1 = getelementptr inbounds %struct.FUM, %struct.FUM* %l_fum, i32 0, i32 1
  %p = getelementptr inbounds %struct.FOO, %struct.FOO* %m1, i32 0, i32 0
  %tmp8 = load i32*, i32** %p, align 4, !tbaa !28, !noalias !32
  %tmp9 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %tmp8, i8* %tmp1, i32** %p, i64 0, metadata !34), !tbaa !28, !noalias !32
  store i32 43, i32* %tmp9, align 4, !tbaa !20, !noalias !32
  %tmp10 = bitcast %struct.FUM* %l_fum to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %tmp10) #5
  ret void
}

; CHECK-LABEL: test03
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 0, metadata ![[SCOPE3:[0-9]+]])
; CHECK:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** null, i64 4, metadata ![[SCOPE3]])
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK_V2:  %tmp3 = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* %a_fum, i8* null, metadata !8, metadata !{{[0-9]+}})
; CHECK-NOT: llvm.noalias.copy.guard
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata ![[SCOPE3_OUT:[0-9]+]])
; CHECK:  %{{.*}} = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* null, i32** %{{.*}}, i64 0, metadata ![[SCOPE3_OUT]])
; CHECK:  %tmp7 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* %0, i32** null, i64 0, metadata ![[SCOPE3]])
; CHECK:  %tmp9 = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* %{{.*}}, i8* %1, i32** null, i64 4, metadata ![[SCOPE3]])
; CHECK: ret void

attributes #0 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="pentium4" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind readnone }
attributes #4 = { argmemonly nounwind speculatable }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{!"clang version"}
!3 = !{!4}
!4 = distinct !{!4, !5, !"test01: unknown scope"}
!5 = distinct !{!5, !"test01"}
!6 = !{!7, !7, i64 0}
!7 = !{!"any pointer", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C/C++ TBAA"}
!10 = !{!11, !4}
!11 = distinct !{!11, !5, !"test01: l_fum"}
!12 = !{!11}
!13 = !{!14, !15}
!14 = !{i32 -1, i32 0}
!15 = !{i32 -1, i32 1, i32 0}
!16 = !{i64 0, i64 4, !6, i64 4, i64 4, !6}
!17 = !{!18, !7, i64 0}
!18 = !{!"FUM", !7, i64 0, !19, i64 4}
!19 = !{!"FOO", !7, i64 0}
!20 = !{!21, !21, i64 0}
!21 = !{!"int", !8, i64 0}
!22 = !{!23}
!23 = distinct !{!23, !24, !"test02: unknown scope"}
!24 = distinct !{!24, !"test02"}
!25 = !{!26, !23}
!26 = distinct !{!26, !24, !"test02: l_fum"}
!27 = !{!26}
!28 = !{!18, !7, i64 4}
!29 = !{!30}
!30 = distinct !{!30, !31, !"test03: unknown scope"}
!31 = distinct !{!31, !"test03"}
!32 = !{!33, !30}
!33 = distinct !{!33, !31, !"test03: l_fum"}
!34 = !{!33}

; CHECK_V1: !0 = !{i32 1, !"NumRegisterParameters", i32 0}
; CHECK_V1: !1 = !{i32 1, !"wchar_size", i32 4}
; CHECK_V1: !2 = !{!"clang version"}
; CHECK_V1: !3 = !{!4}
; CHECK_V1: !4 = distinct !{!4, !5, !"test01: unknown scope"}
; CHECK_V1: !5 = distinct !{!5, !"test01"}
; CHECK_V1: !6 = !{!7}
; CHECK_V1: !7 = distinct !{!7, !5, !"test01: l_fum"}
; CHECK_V1: !8 = !{i64 0, i64 4, !9, i64 4, i64 4, !9}
; CHECK_V1: !9 = !{!10, !10, i64 0}
; CHECK_V1: !10 = !{!"any pointer", !11, i64 0}
; CHECK_V1: !11 = !{!"omnipotent char", !12, i64 0}
; CHECK_V1: !12 = !{!"Simple C/C++ TBAA"}
; CHECK_V1: !13 = !{!7, !4}
; CHECK_V1: !14 = !{!15, !10, i64 0}
; CHECK_V1: !15 = !{!"FUM", !10, i64 0, !16, i64 4}
; CHECK_V1: !16 = !{!"FOO", !10, i64 0}
; CHECK_V1: !17 = !{!18, !18, i64 0}
; CHECK_V1: !18 = !{!"int", !11, i64 0}
; CHECK_V1: !19 = !{!20}
; CHECK_V1: !20 = distinct !{!20, !21, !"test02: unknown scope"}
; CHECK_V1: !21 = distinct !{!21, !"test02"}
; CHECK_V1: !22 = !{!23}
; CHECK_V1: !23 = distinct !{!23, !21, !"test02: l_fum"}
; CHECK_V1: !24 = !{!23, !20}
; CHECK_V1: !25 = !{!15, !10, i64 4}
; CHECK_V1: !26 = !{!27}
; CHECK_V1: !27 = distinct !{!27, !28, !"test03: unknown scope"}
; CHECK_V1: !28 = distinct !{!28, !"test03"}
; CHECK_V1: !29 = !{!30}
; CHECK_V1: !30 = distinct !{!30, !28, !"test03: l_fum"}
; CHECK_V1: !31 = !{!30, !27}

; CHECK_V2: !0 = !{i32 1, !"NumRegisterParameters", i32 0}
; CHECK_V2: !1 = !{i32 1, !"wchar_size", i32 4}
; CHECK_V2: !2 = !{!"clang version"}
; CHECK_V2: !3 = !{!4}
; CHECK_V2: !4 = distinct !{!4, !5, !"test01: unknown scope"}
; CHECK_V2: !5 = distinct !{!5, !"test01"}
; CHECK_V2: !6 = !{!7}
; CHECK_V2: !7 = distinct !{!7, !5, !"test01: l_fum"}
; CHECK_V2: !8 = !{!9, !10}
; CHECK_V2: !9 = !{i32 -1, i32 0}
; CHECK_V2: !10 = !{i32 -1, i32 1, i32 0}
; CHECK_V2: !11 = !{!12, !13, i64 0}
; CHECK_V2: !12 = !{!"FUM", !13, i64 0, !16, i64 4}
; CHECK_V2: !13 = !{!"any pointer", !14, i64 0}
; CHECK_V2: !14 = !{!"omnipotent char", !15, i64 0}
; CHECK_V2: !15 = !{!"Simple C/C++ TBAA"}
; CHECK_V2: !16 = !{!"FOO", !13, i64 0}
; CHECK_V2: !17 = !{!7, !4}
; CHECK_V2: !18 = !{!19, !19, i64 0}
; CHECK_V2: !19 = !{!"int", !14, i64 0}
; CHECK_V2: !20 = !{!21}
; CHECK_V2: !21 = distinct !{!21, !22, !"test02: unknown scope"}
; CHECK_V2: !22 = distinct !{!22, !"test02"}
; CHECK_V2: !23 = !{!24}
; CHECK_V2: !24 = distinct !{!24, !22, !"test02: l_fum"}
; CHECK_V2: !25 = !{!12, !13, i64 4}
; CHECK_V2: !26 = !{!24, !21}
; CHECK_V2: !27 = !{!28}
; CHECK_V2: !28 = distinct !{!28, !29, !"test03: unknown scope"}
; CHECK_V2: !29 = distinct !{!29, !"test03"}
; CHECK_V2: !30 = !{!31}
; CHECK_V2: !31 = distinct !{!31, !29, !"test03: l_fum"}
; CHECK_V2: !32 = !{!31, !28}

; CHECK_V3: !0 = !{i32 1, !"NumRegisterParameters", i32 0}
; CHECK_V3: !1 = !{i32 1, !"wchar_size", i32 4}
; CHECK_V3: !2 = !{!"clang version"}
; CHECK_V3: !3 = !{!4}
; CHECK_V3: !4 = distinct !{!4, !5, !"test01: unknown scope"}
; CHECK_V3: !5 = distinct !{!5, !"test01"}
; CHECK_V3: !6 = !{!7}
; CHECK_V3: !7 = distinct !{!7, !5, !"test01: l_fum"}
; CHECK_V3: !8 = !{!9, !10, i64 0}
; CHECK_V3: !9 = !{!"FUM", !10, i64 0, !13, i64 4}
; CHECK_V3: !10 = !{!"any pointer", !11, i64 0}
; CHECK_V3: !11 = !{!"omnipotent char", !12, i64 0}
; CHECK_V3: !12 = !{!"Simple C/C++ TBAA"}
; CHECK_V3: !13 = !{!"FOO", !10, i64 0}
; CHECK_V3: !14 = !{!7, !4}
; CHECK_V3: !15 = !{!16, !16, i64 0}
; CHECK_V3: !16 = !{!"int", !11, i64 0}
; CHECK_V3: !17 = !{!18}
; CHECK_V3: !18 = distinct !{!18, !19, !"test02: unknown scope"}
; CHECK_V3: !19 = distinct !{!19, !"test02"}
; CHECK_V3: !20 = !{!21}
; CHECK_V3: !21 = distinct !{!21, !19, !"test02: l_fum"}
; CHECK_V3: !22 = !{!9, !10, i64 4}
; CHECK_V3: !23 = !{!21, !18}
; CHECK_V3: !24 = !{!25}
; CHECK_V3: !25 = distinct !{!25, !26, !"test03: unknown scope"}
; CHECK_V3: !26 = distinct !{!26, !"test03"}
; CHECK_V3: !27 = !{!28}
; CHECK_V3: !28 = distinct !{!28, !26, !"test03: l_fum"}
; CHECK_V3: !29 = !{!28, !25}
