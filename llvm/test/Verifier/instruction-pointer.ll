; RUN: not opt -S -passes=verify < %s 2>&1 | FileCheck %s

target datalayout = "P1"

; CHECK: llvm.instruction.pointer must return a pointer in the program address space
; CHECK-NEXT: %ip = call addrspace(1) ptr @llvm.instruction.pointer.p0()
define ptr @wrong_addrspace() {
  %ip = call ptr @llvm.instruction.pointer.p0()
  ret ptr %ip
}

; CHECK-NOT: must return a pointer
define ptr addrspace(1) @right_addrspace() {
  %ip = call ptr addrspace(1) @llvm.instruction.pointer.p1()
  ret ptr addrspace(1) %ip
}
