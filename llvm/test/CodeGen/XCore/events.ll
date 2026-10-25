; RUN: llc < %s -mtriple=xcore | FileCheck %s

declare void @llvm.xcore.setv.p1(ptr addrspace(1) %r, ptr %p)
declare ptr @llvm.xcore.waitevent()
declare ptr @llvm.xcore.checkevent(ptr)
declare void @llvm.xcore.clre()

define i32 @f(ptr addrspace(1) %r) nounwind {
; CHECK-LABEL: f:
entry:
; CHECK: clre
  call void @llvm.xcore.clre()
  call void @llvm.xcore.setv.p1(ptr addrspace(1) %r, ptr edgeaddress(@f, 0))
  call void @llvm.xcore.setv.p1(ptr addrspace(1) %r, ptr edgeaddress(@f, 1))
  %goto_addr = call ptr @llvm.xcore.waitevent()
; CHECK: waiteu
  indirectswitch ptr %goto_addr, [ ptr edgeaddress(@f, 0), label %L1 ptr edgeaddress(@f, 1), label %L2 ]
L1:
  br label %ret
L2:
  br label %ret
ret:
  %retval = phi i32 [1, %L1], [2, %L2]
  ret i32 %retval
}

define i32 @g(ptr addrspace(1) %r) nounwind {
; CHECK-LABEL: g:
entry:
; CHECK: clre
  call void @llvm.xcore.clre()
  call void @llvm.xcore.setv.p1(ptr addrspace(1) %r, ptr edgeaddress(@f, 0))
  %goto_addr = call ptr @llvm.xcore.checkevent(ptr edgeaddress(@f, 1))
; CHECK: setsr 1
; CHECK: clrsr 1
  indirectswitch ptr %goto_addr, [ ptr edgeaddress(@g, 0), label %L1 ptr edgeaddress(@g, 1), label %L2 ]
L1:
  br label %ret
L2:
  br label %ret
ret:
  %retval = phi i32 [1, %L1], [2, %L2]
  ret i32 %retval
}
