; RUN: not llc -mtriple=wasm32-unknown-unknown < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=wasm64-unknown-unknown < %s 2>&1 | FileCheck %s
; RUN: not llc -mtriple=wasm32-unknown-unknown -O0 < %s 2>&1 | FileCheck %s

; WebAssembly has no addressable code, so llvm.instruction.pointer is not
; supported.

; CHECK: error: <unknown>:0:0: llvm.instruction.pointer is not supported on this target.

define ptr @ip() {
  %ip = call ptr @llvm.instruction.pointer.p0()
  ret ptr %ip
}
