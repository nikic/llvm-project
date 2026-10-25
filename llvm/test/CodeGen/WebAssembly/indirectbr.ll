; RUN: llc < %s -asm-verbose=false -disable-wasm-fallthrough-return-opt -wasm-disable-explicit-locals -wasm-keep-registers | FileCheck %s

; This tests that indirectbr instructions are lowered to switches. Currently we
; just reuse the IndirectBrExpand Pass; it has its own IR-level test.
; So this test just ensures that the pass gets run and we can lower indirectbr

target triple = "wasm32"

@test1.targets = constant [4 x ptr] [ptr edgeaddress(@test1, 0),
                                     ptr edgeaddress(@test1, 1),
                                     ptr edgeaddress(@test1, 2),
                                     ptr blockaddress(@test1, %bb3)]

; Just check the barest skeleton of the structure
; CHECK-LABEL: test1:
; CHECK: i32.load
; CHECK: i32.load
; CHECK: loop
; CHECK: block
; CHECK: block
; CHECK: block
; CHECK: block
; CHECK: br_table ${{[^,]+}}, 1, 2, 0
; CHECK: end_block
; CHECK: end_block
; CHECK: end_block
; CHECK: end_block
; CHECK: br
; CHECK: end_loop
; CHECK: end_function
; CHECK: test1.targets:
; CHECK-NEXT: .int32
; CHECK-NEXT: .int32
; CHECK-NEXT: .int32
; CHECK-NEXT: .int32

define void @test1(ptr readonly %p, ptr %sink) #0 {

entry:
  %i0 = load i32, ptr %p
  %target.i0 = getelementptr [4 x ptr], ptr @test1.targets, i32 0, i32 %i0
  %target0 = load ptr, ptr %target.i0
  ; Only a subset of blocks are viable successors here.
  br label %dispatch


bb0:
  store volatile i32 0, ptr %sink
  br label %latch

bb1:
  store volatile i32 1, ptr %sink
  br label %latch

bb2:
  store volatile i32 2, ptr %sink
  br label %latch

bb3:
  store volatile i32 3, ptr %sink
  br label %latch

latch:
  %i.next = load i32, ptr %p
  %target.i.next = getelementptr [4 x ptr], ptr @test1.targets, i32 0, i32 %i.next
  %target.next = load ptr, ptr %target.i.next
  ; A different subset of blocks are viable successors here.
  br label %dispatch

dispatch:
  %dispatch.addr = phi ptr [ %target0, %entry ], [ %target.next, %latch ]
  indirectswitch ptr %dispatch.addr, [
    ptr edgeaddress(@test1, 0), label %bb0
    ptr edgeaddress(@test1, 1), label %bb1
    ptr edgeaddress(@test1, 2), label %bb2
  ]
}
