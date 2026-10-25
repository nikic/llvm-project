; RUN: opt -passes=loop-unroll-and-jam -allow-unroll-and-jam -S < %s | FileCheck %s
;
; Verify that loops whose headers' addresses are taken are unrolled and jammed
; (a blockaddress is data-only).
;
target datalayout = "e-m:e-p:32:32-i64:64-v128:64:128-a:0:32-n32-S64"

@tbl = global [2 x ptr] [ptr blockaddress(@address_taken, %for.outer), ptr blockaddress(@address_taken, %for.inner)]

; CHECK-LABEL: @address_taken(
; CHECK: load
; CHECK: load
; CHECK-NOT: load
; CHECK: br i1
define void @address_taken(i32 %I, i32 %J, ptr noalias nocapture %A, ptr noalias nocapture readonly %B) {
entry:
  %cmp = icmp ne i32 %J, 0
  %cmp122 = icmp ne i32 %I, 0
  %or.cond = and i1 %cmp, %cmp122
  br i1 %or.cond, label %for.outer.preheader, label %for.end

for.outer.preheader:
  br label %for.outer

for.outer:
  %i.us = phi i32 [ %add8.us, %for.latch ], [ 0, %for.outer.preheader ]
  br label %for.inner

for.inner:
  %j.us = phi i32 [ 0, %for.outer ], [ %inc.us, %for.inner ]
  %sum1.us = phi i32 [ 0, %for.outer ], [ %add.us, %for.inner ]
  %arrayidx.us = getelementptr inbounds i32, ptr %B, i32 %j.us
  %0 = load i32, ptr %arrayidx.us, align 4
  %add.us = add i32 %0, %sum1.us
  %inc.us = add nuw i32 %j.us, 1
  %exitcond = icmp eq i32 %inc.us, %J
  br i1 %exitcond, label %for.latch, label %for.inner

for.latch:
  %add.us.lcssa = phi i32 [ %add.us, %for.inner ]
  %arrayidx6.us = getelementptr inbounds i32, ptr %A, i32 %i.us
  store i32 %add.us.lcssa, ptr %arrayidx6.us, align 4
  %add8.us = add nuw i32 %i.us, 1
  %exitcond25 = icmp eq i32 %add8.us, %I
  br i1 %exitcond25, label %for.end.loopexit, label %for.outer, !llvm.loop !0

for.end.loopexit:
  br label %for.end

for.end:
  ret void
}

!0 = distinct !{!0, !{!"llvm.loop.unroll_and_jam.count", i32 2}}
