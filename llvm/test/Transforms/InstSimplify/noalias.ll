; RUN: opt -instsimplify -S < %s | FileCheck %s

define void @test01(i8* %ptr) {
  call i8* @llvm.noalias.p0i8.p0i8.p0p0i8.i32(i8* %ptr, i8* null, i8** null, i32 0, metadata !1)
  ret void

; CHECK-LABEL: @test01
; CHECK-NOT: llvm.noalias.p0i8
; CHECK: ret void
}

define i8* @test02() {
  %v = call i8* @llvm.noalias.p0i8.p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test02
; CHECK: llvm.noalias.p0i8
; CHECK: ret i8* %v
}

define i8* @test03() {
  %v = call i8* @llvm.noalias.p0i8.p0i8.p0p0i8.i32(i8* undef, i8* null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test03
; CHECK-NOT: llvm.noalias.p0i8
; CHECK: ret i8* undef
}

declare i8*  @llvm.noalias.p0i8.p0i8.p0p0i8.i32(i8*, i8*, i8**, i32, metadata ) nounwind

define void @test11(i8* %ptr) {
  call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* %ptr, i8* null, i8** null, i8** null, i32 0, metadata !1)
  ret void

; CHECK-LABEL: @test11
; CHECK-NOT: llvm.provenance.noalias.p0i8
; CHECK: ret void
}

define i8* @test12() {
  %v = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test12
; CHECK: llvm.provenance.noalias.p0i8
; CHECK: ret i8* %v
}

define i8* @test13() {
  %v = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* undef, i8* null, i8** null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test13
; CHECK-NOT: llvm.provenance.noalias.p0i8
; CHECK: ret i8* undef
}

define i8* @test14() {
  %u = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i8** null, i32 0, metadata !1)
  %v = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* %u, i8* null, i8** null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test14
; CHECK: llvm.provenance.noalias.p0i8
; CHECK-NOT: llvm.provenance.noalias.p0i8
; CHECK: ret i8* %u
}

define i8* @test15() {
  %u = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i8** null, i32 1, metadata !1)
  %v = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* %u, i8* null, i8** null, i8** null, i32 0, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test15
; CHECK: llvm.provenance.noalias.p0i8
; CHECK: llvm.provenance.noalias.p0i8
; CHECK: ret i8* %v
}

define i8* @test20() {
  %u = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i8** null, i32 0, metadata !1)
  %v = call i8* @llvm.noalias.arg.guard.p0i8.p0i8(i8* null, i8* %u)
  ret i8* %v

; CHECK-LABEL: @test20
; CHECK: llvm.provenance.noalias.p0i8
; CHECK: llvm.noalias.arg.guard.p0i8
; CHECK: ret i8* %v
}

define i8* @test21() {
  %u = call i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8* null, i8* null, i8** null, i8** null, i32 0, metadata !1)
  %v = call i8* @llvm.noalias.arg.guard.p0i8.p0i8(i8* undef, i8* %u)
  ret i8* %v

; CHECK-LABEL: @test21
; CHECK-NOT: llvm.provenance.noalias.p0i8
; CHECK-NOT: llvm.noalias.arg.guard.p0i8
; CHECK: ret i8* undef
}

define i8* @test30() {
  %v = call i8* @llvm.noalias.copy.guard.p0i8.p0i8(i8* null, i8* null, metadata !2, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test30
; CHECK: llvm.noalias.copy.guard.p0i8
; CHECK: ret i8* %v
}

define void @test31() {
  %v = call i8* @llvm.noalias.copy.guard.p0i8.p0i8(i8* null, i8* null, metadata !2, metadata !1)
  ret void

; CHECK-LABEL: @test31
; CHECK-NOT: llvm.noalias.copy.guard.p0i8
; CHECK: ret void
}

define i8* @test32() {
  %v = call i8* @llvm.noalias.copy.guard.p0i8.p0i8(i8* undef, i8* null, metadata !2, metadata !1)
  ret i8* %v

; CHECK-LABEL: @test32
; CHECK-NOT: llvm.noalias.copy.guard.p0i8
; CHECK: ret i8* undef
}

define void @test40() {
  %v = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i8** null, i32 0, metadata !1)
  ret void

; CHECK-LABEL: @test40
; CHECK-NOT: llvm.noalias.decl.p0i8
; CHECK: ret void
}

define void @test41() {
  %u = alloca i8*
  %v = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i8** %u, i32 0, metadata !4)
  ret void

; CHECK-LABEL: @test41
; CHECK-NOT: alloca
; CHECK-NOT: llvm.noalias.decl.p0i8
; CHECK: ret void
}

define i8** @test42() {
  %u = alloca i8*
  %v = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i8** %u, i32 0, metadata !0)
  ret i8** %u

; CHECK-LABEL: @test42
; CHECK: alloca
; CHECK: llvm.noalias.decl.p0i8
; CHECK: ret i8** %u
}


declare i8* @llvm.provenance.noalias.p0i8.p0i8.p0p0i8.p0p0i8.i32(i8*, i8*, i8**, i8**, i32, metadata ) nounwind
declare i8* @llvm.noalias.arg.guard.p0i8.p0i8(i8*, i8*) nounwind readnone
declare i8* @llvm.noalias.copy.guard.p0i8.p0i8(i8*, i8*, metadata, metadata)
declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i8**, i32, metadata) argmemonly nounwind

!0 = !{!0, !"some domain"}
!1 = !{!1, !0, !"some scope"}
!2 = !{!3}
!3 = !{ i64 -1, i64 0 }
!4 = !{!4, !"some other domain"}
!5 = !{!5, !4, !"some other scope"}
