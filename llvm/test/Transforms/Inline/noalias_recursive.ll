; RUN: opt -inline -enable-noalias-to-md-conversion -use-noalias-intrinsic-during-inlining=1 -S < %s | FileCheck %s

%class.ah = type { [8 x i8] }

; Test for self recursion:

; Function Attrs: nounwind uwtable
define void @Test01(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !1 {
entry:
  %switch = icmp eq i32 %n, 0
  br i1 %switch, label %sw.bb, label %sw.bb1

sw.bb:                                            ; preds = %entry
  %0 = getelementptr inbounds %class.ah, %class.ah* %agg.result, i64 0, i32 0, i64 0
  store i8 42, i8* %0, !noalias !1
  ret void

sw.bb1:                                           ; preds = %entry
  call void @Test01(%class.ah* nonnull sret align 8 %agg.result, i32 0), !noalias !1
  ret void
}

; CHECK-LABEL: define void @Test01(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !1 {
; CHECK: sw.bb1:
; CHECK-NEXT:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0s_class.ahs.i64(%class.ah** null, i64 0, metadata !4)
; CHECK-NEXT:  %2 = call %class.ah* @llvm.noalias.p0s_class.ahs.p0i8.p0p0s_class.ahs.i64(%class.ah* %agg.result, i8* %1, %class.ah** null, i64 0, metadata !4), !noalias !1
; CHECK-NEXT:  %3 = getelementptr inbounds %class.ah, %class.ah* %2, i64 0, i32 0, i64 0
; CHECK-NEXT:  store i8 42, i8* %3, align 1, !noalias !7
; CHECK-NEXT:  ret void


; And equivalent version, but without the selfrecursion:

; Function Attrs: nounwind uwtable
declare void @Test02c(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0

; Function Attrs: nounwind uwtable
define void @Test02b(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !4 {
entry:
  %switch = icmp eq i32 %n, 0
  br i1 %switch, label %sw.bb, label %sw.bb1

sw.bb:                                            ; preds = %entry
  %0 = getelementptr inbounds %class.ah, %class.ah* %agg.result, i64 0, i32 0, i64 0
  store i8 42, i8* %0, !noalias !4
  ret void

sw.bb1:                                           ; preds = %entry
  call void @Test02c(%class.ah* nonnull sret align 8 %agg.result, i32 0), !noalias !4
  ret void
}

; CHECK-LABEL: define void @Test02b(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !10

; Function Attrs: nounwind uwtable
define void @Test02a(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !7 {
entry:
  %switch = icmp eq i32 %n, 0
  br i1 %switch, label %sw.bb, label %sw.bb1

sw.bb:                                            ; preds = %entry
  %0 = getelementptr inbounds %class.ah, %class.ah* %agg.result, i64 0, i32 0, i64 0
  store i8 42, i8* %0, !noalias !7
  ret void

sw.bb1:                                           ; preds = %entry
  call void @Test02b(%class.ah* nonnull sret align 8 %agg.result, i32 0), !noalias !7
  ret void
}

; CHECK-LABEL: define void @Test02a(%class.ah* noalias sret align 8 %agg.result, i32 %n) local_unnamed_addr #0 !noalias !13
; CHECK: sw.bb1:
; CHECK-NEXT:  %1 = call i8* @llvm.noalias.decl.p0i8.p0p0s_class.ahs.i64(%class.ah** null, i64 0, metadata !16)
; CHECK-NEXT:  %2 = call %class.ah* @llvm.noalias.p0s_class.ahs.p0i8.p0p0s_class.ahs.i64(%class.ah* %agg.result, i8* %1, %class.ah** null, i64 0, metadata !16), !noalias !13
; CHECK-NEXT:  %3 = getelementptr inbounds %class.ah, %class.ah* %2, i64 0, i32 0, i64 0
; CHECK-NEXT:  store i8 42, i8* %3, align 1, !noalias !19
; CHECK-NEXT:  ret void

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang"}
!1 = !{!2}
!2 = distinct !{!2, !3, !"Test01: unknown function scope"}
!3 = distinct !{!3, !"Test01"}
!4 = !{!5}
!5 = distinct !{!5, !6, !"Test02b: unknown function scope"}
!6 = distinct !{!6, !"Test02b"}
!7 = !{!8}
!8 = distinct !{!8, !9, !"Test02a: unknown function scope"}
!9 = distinct !{!9, !"Test02a"}

; CHECK: !0 = !{!"clang"}
; CHECK-NEXT: !1 = !{!2}
; CHECK-NEXT: !2 = distinct !{!2, !3, !"Test01: unknown function scope"}
; CHECK-NEXT: !3 = distinct !{!3, !"Test01"}
; CHECK-NEXT: !4 = !{!5}
; CHECK-NEXT: !5 = distinct !{!5, !6, !"Test01: %agg.result"}
; CHECK-NEXT: !6 = distinct !{!6, !"Test01"}
; CHECK-NEXT: !7 = !{!8, !2, !5}
; CHECK-NEXT: !8 = distinct !{!8, !9, !"Test01: unknown function scope"}
; CHECK-NEXT: !9 = distinct !{!9, !"Test01"}
; CHECK-NEXT: !10 = !{!11}
; CHECK-NEXT: !11 = distinct !{!11, !12, !"Test02b: unknown function scope"}
; CHECK-NEXT: !12 = distinct !{!12, !"Test02b"}
; CHECK-NEXT: !13 = !{!14}
; CHECK-NEXT: !14 = distinct !{!14, !15, !"Test02a: unknown function scope"}
; CHECK-NEXT: !15 = distinct !{!15, !"Test02a"}
; CHECK-NEXT: !16 = !{!17}
; CHECK-NEXT: !17 = distinct !{!17, !18, !"Test02b: %agg.result"}
; CHECK-NEXT: !18 = distinct !{!18, !"Test02b"}
; CHECK-NEXT: !19 = !{!14, !17}
