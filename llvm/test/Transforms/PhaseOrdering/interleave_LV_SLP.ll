; REQUIRES: powerpc-registered-target
; RUN: opt < %s -O2 -mtriple=powerpc64le-unknown-linux -S -mcpu=pwr9 -interleave-small-loop-scalar-reduction=true 2>&1 | FileCheck %s
; RUN: opt < %s -passes='default<O2>' -mtriple=powerpc64le-unknown-linux -S -mcpu=pwr9 -interleave-small-loop-scalar-reduction=true 2>&1 | FileCheck %s

;void fun(Vector<double>       &MatrixB,
;         const Vector<double> &MatrixA,
;         const unsigned int * const start,
;         const unsigned int * const end,
;         const double * val) const
;{
;  const unsigned int N=MatrixB.size();
;  MatrixB = MatrixA;
;  for (unsigned int row=0; row<N; ++row)
;  {
;    double sum = 0;
;    for (const unsigned int * col=start; col!=end; ++col, ++val)
;      sum += *val * MatrixB(*col);
;    MatrixB(row) -= sum;
;  }
;}

; CHECK-LABEL: vector.body

; CHECK: load <4 x double>, <4 x double>*

; CHECK: fmul fast <4 x double>

; CHECK: fadd fast <4 x double>

target datalayout = "e-m:e-i64:64-n32:64"
target triple = "powerpc64le-unknown-linux-gnu"

%0 = type { %1, %8 }
%1 = type { %2, i8, double, %4, %7* }
%2 = type <{ i32 (...)**, %3, double*, i32 }>
%3 = type { %7*, i8* }
%4 = type { %5 }
%5 = type { %6 }
%6 = type { i32**, i32**, i32** }
%7 = type <{ %8, i32, i32, i32, [4 x i8], i64, i32, [4 x i8], i64*, i32*, i8, i8, [6 x i8] }>
%8 = type { i32 (...)**, i32, %9, %16* }
%9 = type { %10 }
%10 = type { %11 }
%11 = type { %12, %14 }
%12 = type { %13 }
%13 = type { i8 }
%14 = type { %15, i64 }
%15 = type { i32, %15*, %15*, %15* }
%16 = type { i32 (...)**, i8* }
%17 = type { %8, i32, i32, double* }

$test = comdat any
define dso_local void @test(%0* %arg, %17* dereferenceable(88) %arg1) comdat align 2 {
  %tmp14 = getelementptr %0, %0* %arg, i64 0, i32 0, i32 3, i32 0, i32 0, i32 0
  %tmp15 = load i32**, i32*** %tmp14, align 8
  %tmp18 = getelementptr inbounds %17, %17* %arg1, i64 0, i32 3
  %tmp19 = load double*, double** %tmp18, align 8
  br label %bb22
bb22:                                             ; preds = %bb33, %bb
  %tmp26 = add i64 0, 1
  %tmp27 = getelementptr inbounds i32, i32* null, i64 %tmp26
  %tmp28 = getelementptr inbounds i32*, i32** %tmp15, i64 undef
  %tmp29 = load i32*, i32** %tmp28, align 8
  %tmp32 = getelementptr inbounds double, double* null, i64 %tmp26
  br label %bb40
bb33:                                             ; preds = %bb40
  %tmp35 = getelementptr inbounds double, double* %tmp19, i64 undef
  %tmp37 = fsub fast double 0.000000e+00, %tmp50
  store double %tmp37, double* %tmp35, align 8
  br label %bb22
bb40:                                             ; preds = %bb40, %bb22
  %tmp41 = phi i32* [ %tmp51, %bb40 ], [ %tmp27, %bb22 ]
  %tmp42 = phi double* [ %tmp52, %bb40 ], [ %tmp32, %bb22 ]
  %tmp43 = phi double [ %tmp50, %bb40 ], [ 0.000000e+00, %bb22 ]
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
  br i1 %tmp53, label %bb33, label %bb40
}
