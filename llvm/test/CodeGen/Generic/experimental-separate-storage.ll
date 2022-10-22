; facebook begin T130678741
; RUN: llc < %s

declare void @llvm.experimental.separate.storage(ptr, ptr)

define void @f() {
  call void @llvm.experimental.separate.storage(i8* undef, i8* undef)
  ret void
}
; facebook end T130678741
