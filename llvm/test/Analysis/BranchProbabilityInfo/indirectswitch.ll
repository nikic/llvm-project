; RUN: opt < %s -passes='print<branch-prob>' -disable-output 2>&1 | FileCheck %s

; Branch weights on indirectswitch are used.

; CHECK: Printing analysis {{.*}} for function 'f':
; CHECK: edge %entry -> %a probability is 0x0ccccccd / 0x80000000 = 10.00%
; CHECK: edge %entry -> %b probability is 0x73333333 / 0x80000000 = 90.00%
define void @f(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %a
                           ptr edgeaddress(@f, 1), label %b ], !prof !0
a:
  ret void
b:
  ret void
}

!0 = !{!"branch_weights", i32 1, i32 9}
