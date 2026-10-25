; Bugzilla: https://bugs.llvm.org/show_bug.cgi?id=33623
; RUN: llvm-diff %s %s

define i32 @foo(ptr) {
entry:
  indirectswitch ptr %0, [ ptr edgeaddress(@foo, 0), label %A ptr edgeaddress(@foo, 1), label %B ptr edgeaddress(@foo, 2), label %entry ]
A:
  ret i32 1
B:
  ret i32 2
}
