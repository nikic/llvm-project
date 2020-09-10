; RUN: opt < %s -convert-noalias -verify -S | FileCheck %s
; RUN: opt < %s -passes=convert-noalias,verify -S | FileCheck %s
; RUN: opt < %s -convert-noalias -verify -convert-noalias -verify -S | FileCheck %s

target datalayout = "e-i8:8:8-i16:16:16-i32:32:32-i64:32:32-f16:16:16-f32:32:32-f64:32:32-p:32:32:32:32:8-s0:32:32-a0:0:32-S32-n16:32-v128:32:32-P0-p0:32:32:32:32:8"

%struct.a = type { i8 }

; Function Attrs: noreturn
define dso_local void @_Z3fooPii(i32* %_f, i32 %g) local_unnamed_addr #0 {
entry:
  %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
  %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_f, i8* %0, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
  %2 = bitcast i32* %1 to %struct.a*
  %3 = bitcast i32* %_f to %struct.a*
  br label %for.cond

for.cond:                                         ; preds = %for.cond, %entry
  %prov.h.0 = phi %struct.a* [ %2, %entry ], [ %h.0.guard, %for.cond ]
  %h.0 = phi %struct.a* [ %3, %entry ], [ %h.0.guard, %for.cond ]
  %h.0.guard = call %struct.a* @llvm.noalias.arg.guard.p0s_struct.as.p0s_struct.as(%struct.a* %h.0, %struct.a* %prov.h.0)
  %4 = getelementptr inbounds %struct.a, %struct.a* %h.0, i32 0, i32 0
  %.unpack = load i8, i8* %4, ptr_provenance %struct.a* %prov.h.0, align 1, !noalias !2
  %5 = insertvalue %struct.a undef, i8 %.unpack, 0
  %call = call i32 @_Z1b1a(%struct.a %5), !noalias !2
  br label %for.cond
}
; CHECK: define dso_local void @_Z3fooPii(i32* %_f, i32 %g) local_unnamed_addr #0 {
; CHECK-NEXT: entry:
; CHECK-NEXT:   %0 = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !2)
; CHECK-NEXT:   %1 = call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %_f, i8* %0, i32** null, i32** undef, i32 0, metadata !2), !tbaa !5, !noalias !2
; CHECK-NEXT:   %2 = bitcast i32* %1 to %struct.a*
; CHECK-NEXT:   %3 = bitcast i32* %_f to %struct.a*
; CHECK-NEXT:   br label %for.cond
; CHECK:      for.cond:                                         ; preds = %for.cond, %entry
; CHECK-NEXT:   %prov.h.0 = phi %struct.a* [ %2, %entry ], [ %prov.h.0, %for.cond ]
; CHECK-NEXT:   %prov.h.01 = phi %struct.a* [ %3, %entry ], [ %prov.h.0, %for.cond ]
; CHECK-NEXT:   %h.0 = phi %struct.a* [ %3, %entry ], [ %h.0, %for.cond ]
; CHECK-NEXT:   %4 = getelementptr inbounds %struct.a, %struct.a* %h.0, i32 0, i32 0
; CHECK-NEXT:   %.unpack = load i8, i8* %4, ptr_provenance %struct.a* %prov.h.0, align 1, !noalias !2
; CHECK-NEXT:   %5 = insertvalue %struct.a undef, i8 %.unpack, 0
; CHECK-NEXT:   %call = call i32 @_Z1b1a(%struct.a %5), !noalias !2
; CHECK-NEXT:   br label %for.cond


; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) #1

declare dso_local i32 @_Z1b1a(%struct.a) local_unnamed_addr #2

; Function Attrs: nounwind readnone
declare %struct.a* @llvm.noalias.arg.guard.p0s_struct.as.p0s_struct.as(%struct.a*, %struct.a*) #3

; Function Attrs: nounwind readnone speculatable
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata) #4

attributes #0 = { noreturn "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readnone }
attributes #4 = { nounwind readnone speculatable }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.0 "}
!2 = !{!3}
!3 = distinct !{!3, !4, !"_Z3fooPii: f"}
!4 = distinct !{!4, !"_Z3fooPii"}
!5 = !{!6, !6, i64 0}
!6 = !{!"any pointer", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
