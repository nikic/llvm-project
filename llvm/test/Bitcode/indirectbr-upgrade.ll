; RUN: llvm-dis < %s.bc | FileCheck %s
; RUN: verify-uselistorder < %s.bc

;; Lazy loading: llvm-dis -materialize-metadata only materializes @single,
;; because a global initializer refers to its block addresses, and the
;; functions referring to its block addresses.
; RUN: llvm-dis -materialize-metadata < %s.bc | FileCheck %s --check-prefix=LAZY

;; Lazy loading in llvm-link and ThinLTO importing.
; RUN: llvm-link %s.bc -S | FileCheck %s --check-prefix=LINK
; RUN: llvm-as %p/Inputs/indirectbr-upgrade-main.ll -o %t.main.bc
; RUN: llvm-link %t.main.bc %s.bc --only-needed -S | FileCheck %s --check-prefix=ONLY-NEEDED
; RUN: opt -module-summary %p/Inputs/indirectbr-upgrade-main.ll -o %t.main.summary.bc
; RUN: llvm-lto -thinlto-action=thinlink -o %t.index.bc %t.main.summary.bc %s.bc
; RUN: llvm-lto -thinlto-action=import -thinlto-index %t.index.bc %t.main.summary.bc \
; RUN:   -exported-symbol=main -o - | llvm-dis | FileCheck %s --check-prefix=IMPORT

; indirectbr-upgrade.ll.bc was produced by running `opt -module-summary` from
; just before the removal of indirectbr on this file.

target triple = "x86_64-unknown-linux-gnu"

; CHECK: @tbl = global [3 x ptr] [ptr edgeaddress(@single, 0), ptr edgeaddress(@single, 1), ptr blockaddress(@single, %d)]
; LAZY: @tbl = global [3 x ptr] [ptr edgeaddress(@single, 0), ptr edgeaddress(@single, 1), ptr blockaddress(@single, %d)]
; LINK: @tbl = global [3 x ptr] [ptr edgeaddress(@single, 0), ptr edgeaddress(@single, 1), ptr blockaddress(@single, %d)]
; LINK-LABEL: define i32 @single(ptr %p)
; LINK-NEXT:  entry:
; LINK-NEXT:    indirectswitch ptr %p, [
; LINK-NEXT:      ptr edgeaddress(@single, 0), label %a
; LINK-NEXT:      ptr edgeaddress(@single, 1), label %b
; LINK-LABEL: define ptr @before()
; LINK-NEXT:    ret ptr edgeaddress(@single, 1)
; LINK-LABEL: define ptr @after()
; LINK-NEXT:    ret ptr edgeaddress(@single, 0)
; LINK-LABEL: define ptr @multiple_addr()
; LINK-NEXT:    ret ptr edgeaddress(@multiple, 2)
; ONLY-NEEDED: @tbl = global [3 x ptr] [ptr edgeaddress(@single, 0), ptr edgeaddress(@single, 1), ptr blockaddress(@single, %d)]
; ONLY-NEEDED-LABEL: define i32 @single(ptr %p)
; ONLY-NEEDED-NEXT:  entry:
; ONLY-NEEDED-NEXT:    indirectswitch ptr %p, [
; ONLY-NEEDED-NEXT:      ptr edgeaddress(@single, 0), label %a
; ONLY-NEEDED-NEXT:      ptr edgeaddress(@single, 1), label %b
@tbl = global [3 x ptr] [ptr blockaddress(@single, %a), ptr blockaddress(@single, %b), ptr blockaddress(@single, %d)]

; Function before @single in the bitcode, with a block address of @single.
; CHECK-LABEL: define ptr @before()
; CHECK-NEXT: ret ptr edgeaddress(@single, 1)
; LAZY-LABEL: define ptr @before()
; LAZY-NEXT: ret ptr edgeaddress(@single, 1)
define ptr @before() {
  ret ptr blockaddress(@single, %b)
}

; CHECK-LABEL: define i32 @single(ptr %p)
; CHECK-NEXT:  entry:
; CHECK-NEXT:    indirectswitch ptr %p, [
; CHECK-NEXT:      ptr edgeaddress(@single, 0), label %a
; CHECK-NEXT:      ptr edgeaddress(@single, 1), label %b
; CHECK-NEXT:    ], !prof ![[PROF:[0-9]+]]
; CHECK:       a:
; CHECK-NEXT:    %pa = phi i32 [ 1, %entry ]
; LAZY-LABEL: define i32 @single(ptr %p)
; LAZY-NEXT:  entry:
; LAZY-NEXT:    indirectswitch ptr %p, [
; LAZY-NEXT:      ptr edgeaddress(@single, 0), label %a
; LAZY-NEXT:      ptr edgeaddress(@single, 1), label %b
; LAZY-NEXT:    ], !prof
define i32 @single(ptr %p) {
entry:
  indirectbr ptr %p, [label %a, label %b, label %a], !prof !0

a:
  %pa = phi i32 [ 1, %entry ], [ 1, %entry ]
  ret i32 %pa

b:
  ret i32 2

d:
  ret i32 3
}

; Function after @single in the bitcode, with a block address of @single.
; CHECK-LABEL: define ptr @after()
; CHECK-NEXT: ret ptr edgeaddress(@single, 0)
; LAZY-LABEL: define ptr @after()
; LAZY-NEXT: ret ptr edgeaddress(@single, 0)
define ptr @after() {
  ret ptr blockaddress(@single, %a)
}

; CHECK-LABEL: define i32 @multiple(ptr %p, ptr %q, i1 %cond, i32 %v)
; CHECK:       x:
; CHECK-NEXT:    %vx = add i32 %v, 1
; CHECK-NEXT:    br label %[[DISPATCH:[0-9]+]]
; CHECK:       y:
; CHECK-NEXT:    br label %[[DISPATCH]]
; CHECK:       a:
; CHECK-NEXT:    %pa = phi i32 [ %[[PA:[0-9]+]], %[[DISPATCH]] ]
; CHECK-NEXT:    ret i32 %pa
; CHECK:       b:
; CHECK-NEXT:    %pb = phi i32 [ %[[PB:[0-9]+]], %[[DISPATCH]] ]
; CHECK-NEXT:    ret i32 %pb
; CHECK:       c:
; CHECK-NEXT:    ret i32 %vx.dispatch
; CHECK:       [[DISPATCH]]:
; CHECK-NEXT:    %vx.dispatch = phi i32 [ poison, %y ], [ %vx, %x ]
; CHECK-NEXT:    %[[ADDR:[0-9]+]] = phi ptr [ %p, %x ], [ %q, %y ]
; CHECK-NEXT:    %[[PA]] = phi i32 [ 1, %x ], [ 7, %y ]
; CHECK-NEXT:    %[[PB]] = phi i32 [ 2, %x ], [ 3, %y ]
; CHECK-NEXT:    indirectswitch ptr %[[ADDR]], [
; CHECK-NEXT:      ptr edgeaddress(@multiple, 0), label %a
; CHECK-NEXT:      ptr edgeaddress(@multiple, 1), label %b
; CHECK-NEXT:      ptr edgeaddress(@multiple, 2), label %c
; CHECK-NEXT:    ]
; LAZY: define i32 @multiple(ptr %0, ptr %1, i1 %2, i32 %3) {}
define i32 @multiple(ptr %p, ptr %q, i1 %cond, i32 %v) {
entry:
  br i1 %cond, label %x, label %y

x:
  %vx = add i32 %v, 1
  indirectbr ptr %p, [label %a, label %b, label %c]

y:
  indirectbr ptr %q, [label %b, label %a]

a:
  %pa = phi i32 [ 1, %x ], [ 7, %y ]
  ret i32 %pa

b:
  %pb = phi i32 [ 2, %x ], [ 3, %y ]
  ret i32 %pb

c:
  ret i32 %vx
}

; CHECK-LABEL: define ptr @multiple_addr()
; CHECK-NEXT: ret ptr edgeaddress(@multiple, 2)
define ptr @multiple_addr() {
  ret ptr blockaddress(@multiple, %c)
}

; IMPORT: define available_externally i32 @other()
define i32 @other() {
  ret i32 42
}

define ptr @get_tbl() {
  ret ptr @tbl
}

; CHECK: ![[PROF]] = !{!"branch_weights", i32 4, i32 2}
!0 = !{!"branch_weights", i32 1, i32 2, i32 3}
