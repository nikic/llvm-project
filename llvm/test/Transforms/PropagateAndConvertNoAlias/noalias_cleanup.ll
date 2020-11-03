; RUN: opt < %s -convert-noalias -verify -S | FileCheck %s

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local void @foo() local_unnamed_addr #0 {
entry:
  br label %for.cond

for.cond:                                         ; preds = %if.then, %entry
  %prov.bar.0 = phi i32* [ undef, %entry ], [ %prov.bar.0, %if.then ]
  br i1 undef, label %for.cond3thread-pre-split, label %if.then

if.then:                                          ; preds = %for.cond
  %0 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %prov.bar.0, i8* undef, i32** null, i32** undef, i64 0, metadata !1)
  br i1 undef, label %for.cond, label %for.body

for.body:                                         ; preds = %for.body, %if.then
  %prov.bar.116 = phi i32* [ %1, %for.body ], [ %prov.bar.0, %if.then ]
  %1 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %prov.bar.116, i8* undef, i32** null, i32** undef, i64 0, metadata !1)
  br label %for.body

for.cond3thread-pre-split:                        ; preds = %for.cond
  br label %for.body5

for.body5:                                        ; preds = %for.body5, %for.cond3thread-pre-split
  %prov.bar.220 = phi i32* [ %2, %for.body5 ], [ %prov.bar.0, %for.cond3thread-pre-split ]
  %2 = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32* %prov.bar.220, i8* undef, i32** null, i32** undef, i64 0, metadata !1)
  br label %for.body5
}

; CHECK-LABEL: @foo
; CHECK: call i32* @llvm.provenance.noalias
; CHECK-NOT: call i32* @llvm.provenance.noalias

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64(i32*, i8*, i32**, i32**, i64, metadata) #1

attributes #0 = { "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }

!llvm.ident = !{!0}

!0 = !{!"clang)"}
!1 = !{!2}
!2 = distinct !{!2, !3, !"foo: bar"}
!3 = distinct !{!3, !"foo"}
