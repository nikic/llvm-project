; RUN: opt -S -mcpu=pwr9 -slp-vectorizer -interleave-small-loop-scalar-reduction=true < %s | FileCheck %s
; RUN: opt -S -mcpu=pwr9 -passes='slp-vectorizer' -interleave-small-loop-scalar-reduction=true < %s | FileCheck %s

; CHECK-LABEL: vector.body

; CHECK: load <4 x double>, <4 x double>*

; CHECK: fmul fast <4 x double>

; CHECK: fadd fast <4 x double>

target datalayout = "e-m:e-i64:64-n32:64"
target triple = "powerpc64le-unknown-linux"

%0 = type { i8 }
%1 = type { %2, %9 }
%2 = type { %3, i8, double, %5, %8* }
%3 = type <{ i32 (...)**, %4, double*, i32 }>
%4 = type { %8*, i8* }
%5 = type { %6 }
%6 = type { %7 }
%7 = type { i32**, i32**, i32** }
%8 = type <{ %9, i32, i32, i32, [4 x i8], i64, i32, [4 x i8], i64*, i32*, i8, i8, [6 x i8] }>
%9 = type { i32 (...)**, i32, %10, %17* }
%10 = type { %11 }
%11 = type { %12 }
%12 = type { %13, %15 }
%13 = type { %14 }
%14 = type { i8 }
%15 = type { %16, i64 }
%16 = type { i32, %16*, %16*, %16* }
%17 = type { i32 (...)**, i8* }
%18 = type { %9, i32, i32, double* }
%19 = type <{ i32 (...)**, %4, double*, i32, [4 x i8], %9 }>

$test0 = comdat any

@0 = internal global %0 zeroinitializer, align 1
@__dso_handle = external hidden global i8
@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ i32, void ()*, i8* } { i32 65535, void ()* @1, i8* null }]
declare void @test3(%0*)
declare void @test4(%0*)
; Function Attrs: nofree nounwind
declare i32 @__cxa_atexit(void (i8*)*, i8*, i8*)
define weak_odr dso_local void @test0(%1* %arg, %18* dereferenceable(88) %arg1, %18* dereferenceable(88) %arg2) local_unnamed_addr comdat align 2 {
bb:
  %tmp = getelementptr inbounds %18, %18* %arg1, i64 0, i32 1
  %tmp3 = load i32, i32* %tmp, align 8
  %tmp4 = bitcast %1* %arg to %19*
  %tmp5 = tail call dereferenceable(128) %8* @test1(%19* %tmp4)
  %tmp6 = getelementptr inbounds %8, %8* %tmp5, i64 0, i32 8
  %tmp7 = load i64*, i64** %tmp6, align 8
  %tmp8 = tail call dereferenceable(128) %8* @test1(%19* %tmp4)
  %tmp9 = getelementptr inbounds %8, %8* %tmp8, i64 0, i32 9
  %tmp10 = load i32*, i32** %tmp9, align 8
  %tmp102 = ptrtoint i32* %tmp10 to i64
  %tmp11 = tail call dereferenceable(88) %18* @test2(%18* nonnull %arg1, %18* nonnull dereferenceable(88) %arg2)
  %tmp12 = icmp eq i32 %tmp3, 0
  br i1 %tmp12, label %bb21, label %bb13

bb13:                                             ; preds = %bb
  %tmp14 = getelementptr %1, %1* %arg, i64 0, i32 0, i32 3, i32 0, i32 0, i32 0
  %tmp15 = load i32**, i32*** %tmp14, align 8
  %tmp16 = getelementptr inbounds %1, %1* %arg, i64 0, i32 0, i32 0, i32 2
  %tmp17 = load double*, double** %tmp16, align 8
  %tmp18 = getelementptr inbounds %18, %18* %arg1, i64 0, i32 3
  %tmp19 = load double*, double** %tmp18, align 8
  %tmp20 = zext i32 %tmp3 to i64
  %0 = sub i64 0, %tmp102
  br label %bb22

bb21.loopexit:                                    ; preds = %bb33
  br label %bb21

bb21:                                             ; preds = %bb21.loopexit, %bb
  ret void

bb22:                                             ; preds = %bb33, %bb13
  %tmp23 = phi i64 [ 0, %bb13 ], [ %tmp38, %bb33 ]
  %tmp24 = getelementptr inbounds i64, i64* %tmp7, i64 %tmp23
  %tmp25 = load i64, i64* %tmp24, align 8
  %tmp26 = add i64 %tmp25, 1
  %tmp27 = getelementptr inbounds i32, i32* %tmp10, i64 %tmp26
  %tmp28 = getelementptr inbounds i32*, i32** %tmp15, i64 %tmp23
  %tmp29 = load i32*, i32** %tmp28, align 8
  %tmp30 = icmp eq i32* %tmp27, %tmp29
  br i1 %tmp30, label %bb33, label %bb31

bb31:                                             ; preds = %bb22
  %tmp32 = getelementptr inbounds double, double* %tmp17, i64 %tmp26
  %scevgep = getelementptr i32, i32* %tmp29, i64 -2
  %scevgep1 = bitcast i32* %scevgep to i8*
  %uglygep = getelementptr i8, i8* %scevgep1, i64 %0
  %1 = mul i64 %tmp25, -4
  %scevgep3 = getelementptr i8, i8* %uglygep, i64 %1
  %scevgep34 = ptrtoint i8* %scevgep3 to i64
  %2 = lshr i64 %scevgep34, 2
  %3 = add nuw nsw i64 %2, 1
  %min.iters.check = icmp ult i64 %3, 4
  br i1 %min.iters.check, label %scalar.ph, label %vector.ph

vector.ph:                                        ; preds = %bb31
  %n.mod.vf = urem i64 %3, 4
  %n.vec = sub i64 %3, %n.mod.vf
  %ind.end = getelementptr i32, i32* %tmp27, i64 %n.vec
  %ind.end6 = getelementptr double, double* %tmp32, i64 %n.vec
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %vec.phi = phi double [ 0.000000e+00, %vector.ph ], [ %36, %vector.body ]
  %vec.phi14 = phi double [ 0.000000e+00, %vector.ph ], [ %37, %vector.body ]
  %vec.phi15 = phi double [ 0.000000e+00, %vector.ph ], [ %38, %vector.body ]
  %vec.phi16 = phi double [ 0.000000e+00, %vector.ph ], [ %39, %vector.body ]
  %4 = add i64 %index, 0
  %next.gep = getelementptr i32, i32* %tmp27, i64 %4
  %5 = add i64 %index, 1
  %next.gep7 = getelementptr i32, i32* %tmp27, i64 %5
  %6 = add i64 %index, 2
  %next.gep8 = getelementptr i32, i32* %tmp27, i64 %6
  %7 = add i64 %index, 3
  %next.gep9 = getelementptr i32, i32* %tmp27, i64 %7
  %8 = add i64 %index, 0
  %next.gep10 = getelementptr double, double* %tmp32, i64 %8
  %9 = add i64 %index, 1
  %next.gep11 = getelementptr double, double* %tmp32, i64 %9
  %10 = add i64 %index, 2
  %next.gep12 = getelementptr double, double* %tmp32, i64 %10
  %11 = add i64 %index, 3
  %next.gep13 = getelementptr double, double* %tmp32, i64 %11
  %12 = load double, double* %next.gep10, align 8
  %13 = load double, double* %next.gep11, align 8
  %14 = load double, double* %next.gep12, align 8
  %15 = load double, double* %next.gep13, align 8
  %16 = load i32, i32* %next.gep, align 4
  %17 = load i32, i32* %next.gep7, align 4
  %18 = load i32, i32* %next.gep8, align 4
  %19 = load i32, i32* %next.gep9, align 4
  %20 = zext i32 %16 to i64
  %21 = zext i32 %17 to i64
  %22 = zext i32 %18 to i64
  %23 = zext i32 %19 to i64
  %24 = getelementptr inbounds double, double* %tmp19, i64 %20
  %25 = getelementptr inbounds double, double* %tmp19, i64 %21
  %26 = getelementptr inbounds double, double* %tmp19, i64 %22
  %27 = getelementptr inbounds double, double* %tmp19, i64 %23
  %28 = load double, double* %24, align 8
  %29 = load double, double* %25, align 8
  %30 = load double, double* %26, align 8
  %31 = load double, double* %27, align 8
  %32 = fmul fast double %28, %12
  %33 = fmul fast double %29, %13
  %34 = fmul fast double %30, %14
  %35 = fmul fast double %31, %15
  %36 = fadd fast double %32, %vec.phi
  %37 = fadd fast double %33, %vec.phi14
  %38 = fadd fast double %34, %vec.phi15
  %39 = fadd fast double %35, %vec.phi16
  %index.next = add i64 %index, 4
  %40 = icmp eq i64 %index.next, %n.vec
  br i1 %40, label %middle.block, label %vector.body

middle.block:                                     ; preds = %vector.body
  %bin.rdx = fadd fast double %37, %36
  %bin.rdx17 = fadd fast double %38, %bin.rdx
  %bin.rdx18 = fadd fast double %39, %bin.rdx17
  %cmp.n = icmp eq i64 %3, %n.vec
  br i1 %cmp.n, label %bb33.loopexit, label %scalar.ph

scalar.ph:                                        ; preds = %middle.block, %bb31
  %bc.resume.val = phi i32* [ %ind.end, %middle.block ], [ %tmp27, %bb31 ]
  %bc.resume.val5 = phi double* [ %ind.end6, %middle.block ], [ %tmp32, %bb31 ]
  %bc.merge.rdx = phi double [ 0.000000e+00, %bb31 ], [ %bin.rdx18, %middle.block ]
  br label %bb40

bb33.loopexit:                                    ; preds = %middle.block, %bb40
  %tmp50.lcssa = phi double [ %tmp50, %bb40 ], [ %bin.rdx18, %middle.block ]
  br label %bb33

bb33:                                             ; preds = %bb33.loopexit, %bb22
  %tmp34 = phi double [ 0.000000e+00, %bb22 ], [ %tmp50.lcssa, %bb33.loopexit ]
  %tmp35 = getelementptr inbounds double, double* %tmp19, i64 %tmp23
  %tmp36 = load double, double* %tmp35, align 8
  %tmp37 = fsub fast double %tmp36, %tmp34
  store double %tmp37, double* %tmp35, align 8
  %tmp38 = add nuw nsw i64 %tmp23, 1
  %tmp39 = icmp eq i64 %tmp38, %tmp20
  br i1 %tmp39, label %bb21.loopexit, label %bb22

bb40:                                             ; preds = %bb40, %scalar.ph
  %tmp41 = phi i32* [ %tmp51, %bb40 ], [ %bc.resume.val, %scalar.ph ]
  %tmp42 = phi double* [ %tmp52, %bb40 ], [ %bc.resume.val5, %scalar.ph ]
  %tmp43 = phi double [ %tmp50, %bb40 ], [ %bc.merge.rdx, %scalar.ph ]
  %tmp44 = load double, double* %tmp42, align 8
  %tmp45 = load i32, i32* %tmp41, align 4
  %tmp46 = zext i32 %tmp45 to i64
  %tmp47 = getelementptr inbounds double, double* %tmp19, i64 %tmp46
  %tmp48 = load double, double* %tmp47, align 8
  %tmp49 = fmul fast double %tmp48, %tmp44
  %tmp50 = fadd fast double %tmp49, %tmp43
  %tmp51 = getelementptr inbounds i32, i32* %tmp41, i64 1
  %tmp52 = getelementptr inbounds double, double* %tmp42, i64 1
  %tmp53 = icmp eq i32* %tmp51, %tmp29
  br i1 %tmp53, label %bb33.loopexit, label %bb40
}
declare dereferenceable(128) %8* @test1(%19*)
declare dereferenceable(88) %18* @test2(%18*, %18* dereferenceable(88))
define internal void @1() section ".text.startup" {
bb:
  tail call void @test3(%0* nonnull @0)
  %tmp = tail call i32 @__cxa_atexit(void (i8*)* bitcast (void (%0*)* @test4 to void (i8*)*), i8* getelementptr inbounds (%0, %0* @0, i64 0, i32 0), i8* nonnull @__dso_handle)
  ret void
}
