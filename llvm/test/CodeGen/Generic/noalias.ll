; RUN: llc < %s

define i32* @test(i32* %p) {
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !0)
  %v = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32* %p, i8* %p.decl, i32** null, i32 0, metadata !0)
  ret i32* %v
}

declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) argmemonly nounwind
declare i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i32(i32*, i8*, i32**, i32, metadata) argmemonly nounwind speculatable

!0 = !{!0, !"some domain"}
!1 = !{!1, !0, !"some scope"}
