; RUN: opt -O1 -S < %s | FileCheck %s
; RUN: opt -O1 -S -enable-new-pm=0 < %s | FileCheck %s
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind uwtable
define void @licm(double** align 8 dereferenceable(8) %_M_start.i, i64 %numElem) {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %k.0 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp ult i64 %k.0, %numElem
  br i1 %cmp, label %for.body, label %for.cond.cleanup

for.body:                                         ; preds = %for.cond
  %0 = load double*, double** %_M_start.i, align 8, !tbaa !3
  %add.ptr.i = getelementptr inbounds double, double* %0, i64 %k.0
  store double 2.000000e+00, double* %add.ptr.i, align 8, !tbaa !8
  %inc = add nuw i64 %k.0, 1
  br label %for.cond

for.cond.cleanup:                                 ; preds = %for.cond
  ret void
}

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{!"clang version 15.0.0 (https://github.com/llvm/llvm-project.git fc510998f7c287df2bc1304673e0cd8452d50b31)"}
!3 = !{!4, !5, i64 0}
!4 = !{!"_ZTSNSt12_Vector_baseIdSaIdEE17_Vector_impl_dataE", !5, i64 0, !5, i64 8, !5, i64 16}
!5 = !{!"any pointer", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = !{!9, !9, i64 0}
!9 = !{!"double", !6, i64 0}

; The LICM prior to Loop-Rotate should not speculate instructions
; lest it lose information it could have kept when running after
; Loop-Rotate

; CHECK: define void @licm(double** nocapture readonly align 8 dereferenceable(8) %_M_start.i, i64 %numElem)
; CHECK-NEXT: entry:
; CHECK-NEXT:   %cmp1.not = icmp eq i64 %numElem, 0
; CHECK-NEXT:   br i1 %cmp1.not, label %for.cond.cleanup, label %for.body.lr.ph

; CHECK: for.body.lr.ph:                                   ; preds = %entry
; CHECK-NEXT:   %{{.*}} = load double*, double** %_M_start.i, align 8, !tbaa !{{0-9:.+}}
; CHECK-NEXT:   br label %for.body
