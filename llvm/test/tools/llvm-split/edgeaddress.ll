; Test that functions are in the same partition as the users of their edge
; addresses.
; RUN: llvm-split -j5 -o %t %s
; RUN: llvm-dis -o - %t0 | FileCheck --check-prefix=CHECK0 %s
; RUN: llvm-dis -o - %t1 | FileCheck --check-prefix=CHECK1234 %s
; RUN: llvm-dis -o - %t2 | FileCheck --check-prefix=CHECK1234 %s
; RUN: llvm-dis -o - %t3 | FileCheck --check-prefix=CHECK1234 %s
; RUN: llvm-dis -o - %t4 | FileCheck --check-prefix=CHECK1234 %s

; CHECK0:    @xxx = global [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@g, 0)]
; CHECK1234: @xxx = external global [2 x ptr]
; CHECK1234-NOT: edgeaddress
@xxx = global [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@g, 0)]

; CHECK0:    define i32 @f(ptr %p)
; CHECK1234: declare i32 @f(ptr)
define i32 @f(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@f, 0), label %exit ]
exit:
  ret i32 0
}

; CHECK0:    define i32 @g(ptr %p)
; CHECK1234: declare i32 @g(ptr)
define i32 @g(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@g, 0), label %exit ]
exit:
  ret i32 0
}

; CHECK0:    define ptr @h()
; CHECK1234: declare ptr @h()
define ptr @h() {
entry:
  ret ptr edgeaddress(@f, 0)
}
