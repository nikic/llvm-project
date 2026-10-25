; RUN: opt -passes='require<profile-summary>,function(codegenprepare)' -S -mtriple=x86_64 < %s | FileCheck %s

; Test that critical edges from an indirectswitch are split correctly when the
; source block is a loop to itself.

; CHECK: while.body:
; CHECK-NEXT: %dest.sroa = phi i32 [ %1, %while.body ], [ undef, %if.else1.while.body_crit_edge ], [ undef, %if.else ]
; CHECK: if.else1:
; CHECK-NEXT: indirectswitch ptr undef, [
; CHECK-NEXT: ptr edgeaddress(@test, 0), label %if.then
; CHECK-NEXT: ptr edgeaddress(@test, 1), label %if.else1.while.body_crit_edge
; CHECK-NEXT: ptr edgeaddress(@test, 2), label %if.else
; CHECK-NEXT: ptr edgeaddress(@test, 3), label %if.else1
; CHECK: if.else1.while.body_crit_edge:
; CHECK-NEXT: br label %while.body

define void @test(i1 %c) {
entry:
  br label %if.else

if.else:
  br i1 %c, label %while.body, label %preheader

preheader:
  br label %if.else1

if.then:
  unreachable

while.body:
  %dest.sroa = phi i32 [ %1, %while.body ], [ undef, %if.else1 ], [ undef, %if.else ]
  %0 = inttoptr i32 %dest.sroa to ptr
  %incdec.ptr = getelementptr inbounds i8, ptr %0, i32 -1
  %1 = ptrtoint ptr %incdec.ptr to i32
  store i8 undef, ptr %incdec.ptr, align 1
  br label %while.body

if.else1:
  indirectswitch ptr undef, [ ptr edgeaddress(@test, 0), label %if.then ptr edgeaddress(@test, 1), label %while.body ptr edgeaddress(@test, 2), label %if.else ptr edgeaddress(@test, 3), label %if.else1 ]
}

