; RUN: opt < %s -instcombine -S | FileCheck %s

target datalayout = "e-i8:8:8-i16:16:16-i32:32:32-i64:32:32-f32:32:32-f64:32:32-p:32:32:32:32:8-s0:32:32-a0:8:8-S32-n32-v128:32:32-P0-p0:32:32:32:32:8-p10:32:32:32:32:8-p20:32:32:32:32:8"

@__const.f1.i = private unnamed_addr constant { i8*, [4 x i8] } { i8* null, [4 x i8] undef }, align 4

declare void @do_something(i8*)

; Function Attrs: nounwind optsize
define dso_local i64 @test01() local_unnamed_addr #0 {
entry:
  %i.sroa.6 = alloca [2 x i8], align 2
  %i.sroa.6.0..sroa_idx6 = getelementptr inbounds [2 x i8], [2 x i8]* %i.sroa.6, i32 0, i32 0
  call void @llvm.lifetime.start.p0i8(i64 2, i8* %i.sroa.6.0..sroa_idx6)
  %0 = call i8* @llvm.noalias.decl.p0i8.p0a2i8.i32([2 x i8]* %i.sroa.6, i32 2, metadata !2)
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 2 %i.sroa.6.0..sroa_idx6, i8* align 2 getelementptr inbounds (i8, i8* bitcast ({ i8*, [4 x i8] }* @__const.f1.i to i8*), i32 2), i32 2, i1 false)
  %i.sroa.6.cast = bitcast [2 x i8]* %i.sroa.6 to i8*
  call void @do_something(i8* %i.sroa.6.cast)
  call void @llvm.lifetime.end.p0i8(i64 2, i8* %i.sroa.6.0..sroa_idx6)
  ret i64 undef
}

; CHECK-LABEL: @test01(
; CHECK:  %i.sroa.6 = alloca i16, align 2
; CHECK:  %0 = call i8* @llvm.noalias.decl.p0i8.p0i16.i32(i16* nonnull %i.sroa.6, i32 2, metadata !2)

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* nocapture writeonly, i8* nocapture readonly, i32, i1 immarg) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare i8* @llvm.noalias.decl.p0i8.p0a2i8.i32([2 x i8]*, i32, metadata) #1

attributes #0 = { nounwind optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang"}
!2 = !{!3}
!3 = distinct !{!3, !4, !"f1: i"}
!4 = distinct !{!4, !"f1"}
