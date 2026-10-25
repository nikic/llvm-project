; RUN: not opt < %s -passes=fix-irreducible -S 2>&1 | FileCheck %s
; CHECK: LLVM ERROR: unsupported block terminator: fix-irreducible only supports br, callbr, and switch instructions

define i32 @test(i1 %cond) {
entry:
  %target = select i1 %cond,
                   ptr edgeaddress(@test, 0),
                   ptr edgeaddress(@test, 1)
  indirectswitch ptr %target, [ ptr edgeaddress(@test, 0), label %then ptr edgeaddress(@test, 1), label %else ]

then:
  br label %else

else:
  br label %then
}
