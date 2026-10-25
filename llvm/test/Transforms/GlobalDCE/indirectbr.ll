; RUN: opt -S -passes=globaldce < %s | FileCheck %s

@L = internal unnamed_addr constant [3 x ptr] [ptr edgeaddress(@test1, 0), ptr edgeaddress(@test1, 1), ptr null], align 16

; CHECK: @L = internal unnamed_addr constant

define void @test1(i32 %idx) {
entry:
  br label %L1

L1:
  %arrayidx = getelementptr inbounds [3 x ptr], ptr @L, i32 0, i32 %idx
  %l = load ptr, ptr %arrayidx
  indirectswitch ptr %l, [ ptr edgeaddress(@test1, 0), label %L1 ptr edgeaddress(@test1, 1), label %L2 ]

L2:
  ret void
}
