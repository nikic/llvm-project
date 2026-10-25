; RUN: llvm-reduce -abort-on-invalid-reduction --delta-passes=basic-blocks --test FileCheck --test-arg --check-prefixes=INTERESTING --test-arg %s --test-arg --input-file %s -o %t
; RUN: FileCheck --check-prefix=RESULT %s < %t

; Check that cases branching to removed blocks are dropped from an
; indirectswitch.

; INTERESTING: store i32 2

; RESULT:      indirectswitch ptr %p, [
; RESULT-NEXT:   ptr edgeaddress(@f, 1), label %b
; RESULT-NEXT: ]
; RESULT-NOT: store i32 1
; RESULT-NOT: store i32 3

define void @f(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 1), label %b
                           ptr edgeaddress(@f, 2), label %c ]
a:
  store i32 1, ptr %p
  ret void
b:
  store i32 2, ptr %p
  ret void
c:
  store i32 3, ptr %p
  ret void
}
