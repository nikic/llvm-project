target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@label_addr = internal constant [1 x ptr] [ptr edgeaddress(@bar, 0)], align 8

define dso_local ptr @foo() {
  ret ptr @label_addr
}

define dso_local ptr @bar(ptr %p) {
entry:
  indirectswitch ptr %p, [ ptr edgeaddress(@bar, 0), label %lb ]

lb:
  ret ptr @label_addr
}

; Uses an edgeaddress of another function.
define dso_local ptr @baz() {
  ret ptr edgeaddress(@bar, 0)
}
