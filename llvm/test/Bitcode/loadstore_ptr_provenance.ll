; RUN: opt --verify -S < %s | FileCheck %s
; RUN: llvm-as < %s | llvm-dis | llvm-as | llvm-dis | FileCheck %s
; RUN: verify-uselistorder < %s

define i32 @f(i32* %p, i32* %q, i32* %word, i32** %base) {
  ; CHECK:      define i32 @f(i32* %p, i32* %q, i32* %word, i32** %base) {
  store i32 42, i32* %p, ptr_provenance i32* %p
  ; CHECK-NEXT:   store i32 42, i32* %p, ptr_provenance i32* %p
  store i32 43, i32* %q, ptr_provenance i32* %q
  ; CHECK-NEXT:   store i32 43, i32* %q, ptr_provenance i32* %q
  %r = load i32, i32* %p, ptr_provenance i32* %p
  ; CHECK-NEXT:   %r = load i32, i32* %p, ptr_provenance i32* %p

  %ld.1p = load atomic i32, i32* %word monotonic, ptr_provenance i32* %word, align 4
  ; CHECK: %ld.1p = load atomic i32, i32* %word monotonic, ptr_provenance i32* %word, align 4
  %ld.2p = load atomic volatile i32, i32* %word acquire, ptr_provenance i32* %word, align 8
  ; CHECK: %ld.2p = load atomic volatile i32, i32* %word acquire, ptr_provenance i32* %word, align 8
  %ld.3p = load atomic volatile i32, i32* %word syncscope("singlethread") seq_cst, ptr_provenance i32* %word, align 16
  ; CHECK: %ld.3p = load atomic volatile i32, i32* %word syncscope("singlethread") seq_cst, ptr_provenance i32* %word, align 16

  store atomic i32 23, i32* %word monotonic, align 4
  ; CHECK: store atomic i32 23, i32* %word monotonic, align 4
  store atomic volatile i32 24, i32* %word monotonic, align 4
  ; CHECK: store atomic volatile i32 24, i32* %word monotonic, align 4
  store atomic volatile i32 25, i32* %word syncscope("singlethread") monotonic, align 4
   ; CHECK: store atomic volatile i32 25, i32* %word syncscope("singlethread") monotonic, align 4

  load i32*, i32** %base, ptr_provenance i32** %base, align 8, !invariant.load !0, !nontemporal !1, !nonnull !0, !dereferenceable !2, !dereferenceable_or_null !2
  ; CHECK: load i32*, i32** %base, ptr_provenance i32** %base, align 8, !invariant.load !0, !nontemporal !1, !nonnull !0, !dereferenceable !2, !dereferenceable_or_null !2
  load volatile i32*, i32** %base, ptr_provenance i32** %base, align 8, !invariant.load !0, !nontemporal !1, !nonnull !0, !dereferenceable !2, !dereferenceable_or_null !2
  ; CHECK: load volatile i32*, i32** %base, ptr_provenance i32** %base, align 8, !invariant.load !0, !nontemporal !1, !nonnull !0, !dereferenceable !2, !dereferenceable_or_null !2

  store i32* null, i32** %base, ptr_provenance i32** %base, align 4, !nontemporal !1
  ; CHECK: store i32* null, i32** %base, ptr_provenance i32** %base, align 4, !nontemporal !1
  store volatile i32* null, i32** %base, ptr_provenance i32** %base, align 4, !nontemporal !1
  ; CHECK: store volatile i32* null, i32** %base, ptr_provenance i32** %base, align 4, !nontemporal !1

  ret i32 %r
  ; CHECK-NEXT:   ret i32 %r
}

!0 = !{i32 1}
!1 = !{}
!2 = !{i64 4}
