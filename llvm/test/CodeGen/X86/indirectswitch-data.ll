; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O2 -relocation-model=static < %s | FileCheck %s --check-prefixes=CHECK,O2
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O2 -relocation-model=pic < %s | FileCheck %s --check-prefixes=CHECK,O2,PIC
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O0 < %s | FileCheck %s --check-prefixes=CHECK,O0
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O2 -stop-after=finalize-isel < %s -o %t.mir
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -start-after=finalize-isel %t.mir -o - | FileCheck %s --check-prefixes=CHECK,O2
; RUN: llc -mtriple=x86_64-unknown-linux-gnu -O2 -mattr=+retpoline-indirect-branches < %s | FileCheck %s --check-prefix=RETPOLINE

; Global initializers referring to edgeaddresses, defined before and after the
; function, refer to the labels of the case destinations. An edgeaddress
; without an indirectswitch case refers to the function. Code in other
; functions loads the edgeaddress from a private global, as it may be code
; generated before the function.

; CHECK-LABEL: before:
; CHECK:         movq .Lf.edgeaddress.3{{(\(%rip\))?}}, %rax
; CHECK-LABEL: before_unused:
; CHECK:         f@GOTPCREL

; CHECK-LABEL: f:
; O2:          [[B:\.Ltmp[0-9]+]]: # Block address taken
; O2-NEXT:     # %dispatch.b_crit_edge
; CHECK:       [[A:\.Ltmp[0-9]+]]: # Block address taken
; CHECK-NEXT:  # %a
; O0:          [[B:\.Ltmp[0-9]+]]: # Block address taken
; O0-NEXT:     # %b
; CHECK:       [[C:\.Ltmp[0-9]+]]: # Block address taken
; CHECK-NEXT:  # %c
; CHECK-LABEL: after:
; CHECK:         movq .Lf.edgeaddress.3{{(\(%rip\))?}}, %rax
; CHECK-LABEL: rel:
; CHECK:       [[RA:\.Ltmp[0-9]+]]: # Block address taken
; CHECK-NEXT:  # %a
; CHECK:       [[RB:\.Ltmp[0-9]+]]: # Block address taken
; CHECK-NEXT:  # %b

; PIC:           .section .data.rel.ro
; CHECK-LABEL: tbl:
; CHECK-NEXT:    .quad [[A]]
; CHECK-NEXT:    .quad [[B]]
; CHECK-NEXT:    .quad [[B]]
; CHECK-NEXT:    .quad [[C]]
; CHECK-LABEL: unused:
; CHECK-NEXT:    .quad f
; Label differences need no relocation.
; PIC:           .section .rodata
; CHECK-LABEL: reltbl:
; CHECK-NEXT:    .long [[RA]]-[[RA]]
; CHECK-NEXT:    .long [[RB]]-[[RA]]
; CHECK-LABEL: tbl2:
; CHECK-NEXT:    .quad [[B]]
; CHECK-NEXT:    .quad [[B]]
; CHECK-LABEL: .Lf.edgeaddress.3:
; CHECK-NEXT:    .quad [[C]]

; With retpolines, the edgeaddresses are replaced with small integers.
; RETPOLINE-LABEL: tbl:
; RETPOLINE-NEXT:    .quad 1
; RETPOLINE-NEXT:    .quad 2
; RETPOLINE-NEXT:    .quad 3
; RETPOLINE-NEXT:    .quad 4
; RETPOLINE-LABEL: unused:
; RETPOLINE-NEXT:    .quad f
; RETPOLINE-LABEL: reltbl:
; RETPOLINE-NEXT:    .long 1-1
; RETPOLINE-NEXT:    .long 2-1
; RETPOLINE-LABEL: tbl2:
; RETPOLINE-NEXT:    .quad 2
; RETPOLINE-NEXT:    .quad 3
; RETPOLINE-LABEL: .Lf.edgeaddress.3:
; RETPOLINE-NEXT:    .quad 4

@tbl = constant [4 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1), ptr edgeaddress(@f, 2), ptr edgeaddress(@f, 3)]

@unused = global ptr edgeaddress(@f, 9)

define ptr @before() {
  ret ptr edgeaddress(@f, 3)
}

define ptr @before_unused() {
  ret ptr edgeaddress(@f, 8)
}

define i32 @f(i32 %i, i32 %x) {
entry:
  %idx = zext i32 %i to i64
  %slot = getelementptr [4 x ptr], ptr @tbl, i64 0, i64 %idx
  %t = load ptr, ptr %slot
  %cmp = icmp eq i32 %x, 0
  br i1 %cmp, label %b, label %dispatch

dispatch:
  indirectswitch ptr %t, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 1), label %b
                           ptr edgeaddress(@f, 2), label %b
                           ptr edgeaddress(@f, 3), label %c ]

a:
  ret i32 1

b:
  %p = phi i32 [ 2, %dispatch ], [ 2, %dispatch ], [ %x, %entry ]
  ret i32 %p

c:
  ret i32 3
}

define ptr @after() {
  ret ptr edgeaddress(@f, 3)
}

@reltbl = constant [2 x i32] [
  i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@rel, 0) to i64), i64 ptrtoint (ptr edgeaddress(@rel, 0) to i64)) to i32),
  i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@rel, 1) to i64), i64 ptrtoint (ptr edgeaddress(@rel, 0) to i64)) to i32)]

define i32 @rel(i32 %i) {
entry:
  %idx = zext i32 %i to i64
  %slot = getelementptr [2 x i32], ptr @reltbl, i64 0, i64 %idx
  %off = load i32, ptr %slot
  %off.ext = sext i32 %off to i64
  %t = getelementptr i8, ptr edgeaddress(@rel, 0), i64 %off.ext
  indirectswitch ptr %t, [ ptr edgeaddress(@rel, 0), label %a
                           ptr edgeaddress(@rel, 1), label %b ]
a:
  ret i32 10
b:
  ret i32 20
}

@tbl2 = global [2 x ptr] [ptr edgeaddress(@f, 1), ptr edgeaddress(@f, 2)]
