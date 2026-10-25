; RUN: llc < %s -mtriple=x86_64-unknown-linux-gnu -mattr=+retpoline | FileCheck %s
;
; verify that blocks are NOT marked as "Block address taken" when the
; BlockAddress constant has no users (was optimized away).
;
; With retpoline enabled, the indirectbr is replaced with direct comparisons
; against constant integer values. The BlockAddress constants in the global
; array become unused (constant-folded to integers), so the blocks should NOT
; be marked as address-taken.

@targets = internal constant [4 x ptr] [
  ptr edgeaddress(@test_stale_addresstaken, 0),
  ptr edgeaddress(@test_stale_addresstaken, 1),
  ptr edgeaddress(@test_stale_addresstaken, 2),
  ptr edgeaddress(@test_stale_addresstaken, 3)
]

define i32 @test_stale_addresstaken(i32 %idx) {
entry:
  %ptr = getelementptr [4 x ptr], ptr @targets, i32 0, i32 %idx
  %dest = load ptr, ptr %ptr
  indirectswitch ptr %dest, [ ptr edgeaddress(@test_stale_addresstaken, 0), label %bb0 ptr edgeaddress(@test_stale_addresstaken, 1), label %bb1 ptr edgeaddress(@test_stale_addresstaken, 2), label %bb2 ptr edgeaddress(@test_stale_addresstaken, 3), label %bb3 ]

; CHECK-LABEL: test_stale_addresstaken:
; CHECK-NOT: Block address taken
; CHECK-LABEL: .Lfunc_end0:

bb0:
  ret i32 0

bb1:
  ret i32 1

bb2:
  ret i32 2

bb3:
  ret i32 3
}
