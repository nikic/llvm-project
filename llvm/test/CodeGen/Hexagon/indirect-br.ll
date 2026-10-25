; RUN: llc -mtriple=hexagon -hexagon-initial-cfg-cleanup=0 < %s | FileCheck %s

; CHECK: jumpr r{{[0-9]+}}

define i32 @check_indirect_br(ptr %target) nounwind {
entry:
  indirectswitch ptr %target, [ ptr edgeaddress(@check_indirect_br, 0), label %test_label ]

test_label:
  br label %ret

ret:
  ret i32 -1
}
