; RUN: opt < %s -passes=pgo-instr-gen -S | FileCheck %s

; Function Attrs: norecurse nounwind readnone uwtable
define i32 @bar(i32 %v) local_unnamed_addr #0 {
entry:
  %mul = shl nsw i32 %v, 1
  ret i32 %mul
}

; Function Attrs: norecurse nounwind readonly uwtable
define i32 @foo(ptr nocapture readonly %p) #1 {
entry:
  %targets = alloca [256 x ptr], align 16
  %arrayidx1 = getelementptr inbounds [256 x ptr], ptr %targets, i64 0, i64 93
  store ptr edgeaddress(@foo, 1), ptr %arrayidx1, align 8
  br label %for.cond2

for.cond2:                                        ; preds = %if.end, %for.cond2, %entry
; CHECK: for.cond2:                                        ; preds = %for.cond2.for.cond2_crit_edge, %if.end, %entry
  %p.addr.0 = phi ptr [ %p, %entry ], [ %incdec.ptr5, %if.end ], [ %incdec.ptr, %for.cond2 ]
  %incdec.ptr = getelementptr inbounds i8, ptr %p.addr.0, i64 1
  %0 = load i8, ptr %p.addr.0, align 1
  %cond = icmp eq i8 %0, 93
  br i1 %cond, label %if.end.preheader, label %for.cond2

if.end.preheader:                                 ; preds = %for.cond2
  br label %if.end

if.end:                                           ; preds = %if.end.preheader, %if.end
; CHECK: if.end:                                           ; preds = %if.end.if.end_crit_edge, %if.end.preheader
  %p.addr.1 = phi ptr [ %incdec.ptr5, %if.end ], [ %incdec.ptr, %if.end.preheader ]
  %incdec.ptr5 = getelementptr inbounds i8, ptr %p.addr.1, i64 1
  %1 = load i8, ptr %p.addr.1, align 1
  %idxprom6 = zext i8 %1 to i64
  %arrayidx7 = getelementptr inbounds [256 x ptr], ptr %targets, i64 0, i64 %idxprom6
  %2 = load ptr, ptr %arrayidx7, align 8
  indirectswitch ptr %2, [ ptr edgeaddress(@foo, 0), label %for.cond2 ptr edgeaddress(@foo, 1), label %if.end ]
; CHECK: indirectswitch ptr %2, [
}

;; Critical edges from an indirectswitch can be split like other critical
;; edges.
; CHECK-LABEL: @cannot_split(
; CHECK:       entry:
; CHECK:         call void @llvm.instrprof.increment(ptr @__profn_cannot_split, i64 {{[0-9]+}}, i32 2, i32 1)
; CHECK:       indirect:
; CHECK-NEXT:    indirectswitch ptr %1, [
; CHECK-NEXT:      ptr edgeaddress(@cannot_split, 0), label %indirect.indirect_crit_edge
; CHECK:       indirect.indirect_crit_edge:
; CHECK-NEXT:    call void @llvm.instrprof.increment(ptr @__profn_cannot_split, i64 {{[0-9]+}}, i32 2, i32 0)
define i32 @cannot_split(ptr nocapture readonly %p) {
entry:
  %targets = alloca <2 x ptr>, align 16
  store <2 x ptr> <ptr edgeaddress(@cannot_split, 0), ptr edgeaddress(@cannot_split, 1)>, ptr %targets, align 16
  %arrayidx2 = getelementptr inbounds i8, ptr %p, i64 1
  %0 = load i8, ptr %arrayidx2
  %idxprom = sext i8 %0 to i64
  %arrayidx3 = getelementptr inbounds <2 x ptr>, ptr %targets, i64 0, i64 %idxprom
  %1 = load ptr, ptr %arrayidx3, align 8
  br label %indirect

indirect:                                         ; preds = %entry, %indirect
  indirectswitch ptr %1, [ ptr edgeaddress(@cannot_split, 0), label %indirect ptr edgeaddress(@cannot_split, 1), label %end ]

end:                                              ; preds = %indirect
  ret i32 0
}
