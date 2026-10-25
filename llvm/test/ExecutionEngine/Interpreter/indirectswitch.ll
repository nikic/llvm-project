; RUN: %lli -jit-kind=mcjit -force-interpreter=true %s

@tbl = constant [3 x ptr] [ptr edgeaddress(@main, 0), ptr edgeaddress(@main, 1), ptr edgeaddress(@main, 2)]

define i32 @main() {
entry:
  br label %dispatch

dispatch:
  %i = phi i64 [ 0, %entry ], [ %i.next, %a ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %a ]
  %slot = getelementptr [3 x ptr], ptr @tbl, i64 0, i64 %i
  %target = load ptr, ptr %slot
  %i.next = add i64 %i, 1
  indirectswitch ptr %target, [ ptr edgeaddress(@main, 0), label %a
                                ptr edgeaddress(@main, 1), label %a
                                ptr edgeaddress(@main, 2), label %exit ]

a:
  %acc.next = add i32 %acc, 1
  br label %dispatch

exit:
  %fail = icmp ne i32 %acc, 2
  %ret = zext i1 %fail to i32
  ret i32 %ret
}
