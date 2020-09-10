; RUN: llc < %s

define i32* @test(i32* %p) {
  %p.decl = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32** null, i32 0, metadata !0)
  %p.provenance = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32* %p, i8* %p.decl, i32** null, i32** undef, i32 0, metadata !0)
  %p.guard = call i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32* %p, i32* %p.provenance)
  ret i32* %p.guard
}

declare i8* @llvm.noalias.decl.p0i8.p0p0i32.i32(i32**, i32, metadata) argmemonly nounwind
declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i32(i32*, i8*, i32**, i32**, i32, metadata)  argmemonly nounwind speculatable
declare i32* @llvm.noalias.arg.guard.p0i32.p0i32(i32*, i32*) nounwind readnone

!0 = !{!0, !"some domain"}
!1 = !{!1, !0, !"some scope"}
