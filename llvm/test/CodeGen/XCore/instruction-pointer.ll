; RUN: llc -mtriple=xcore -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=xcore -O0 -verify-machineinstrs < %s | FileCheck %s

; ldap can only write r11, so r11 is clobbered. The result is the address of
; the instruction following the ldap.

; CHECK-LABEL: ip:
; CHECK:         ldap r11, [[L:\.Ltmp[0-9]+]]
; CHECK-NEXT:  [[L]]:
; CHECK-NEXT:    mov r0, r11
; CHECK-NEXT:    retsp 0
define ptr @ip() {
  %ip = call ptr @llvm.instruction.pointer.p0()
  ret ptr %ip
}

; The two calls must not be combined.
; CHECK-LABEL: ip_twice:
; CHECK:         ldap r11, [[L1:\.Ltmp[0-9]+]]
; CHECK-NEXT:  [[L1]]:
; CHECK-NEXT:    mov [[R1:r[0-9]+]], r11
; CHECK-NEXT:    stw [[R1]], {{r[0-9]+}}[0]
; CHECK:         bl foo
; CHECK:         ldap r11, [[L2:\.Ltmp[0-9]+]]
; CHECK-NEXT:  [[L2]]:
; CHECK-NEXT:    mov [[R2:r[0-9]+]], r11
; CHECK-NEXT:    stw [[R2]], {{r[0-9]+}}[0]
define void @ip_twice(ptr %out) {
  %ip1 = call ptr @llvm.instruction.pointer.p0()
  store ptr %ip1, ptr %out
  call void @foo()
  %ip2 = call ptr @llvm.instruction.pointer.p0()
  store ptr %ip2, ptr %out
  ret void
}

declare void @foo()
