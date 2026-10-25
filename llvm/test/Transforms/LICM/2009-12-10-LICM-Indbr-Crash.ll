; Test for rdar://7452967
; RUN: opt < %s -passes=licm -disable-output
define void @foo (ptr %arg)
{
  entry:
    br i1 false, label %preheader, label %return

  preheader:
    br i1 false, label %loop, label %return

  loop:
    indirectswitch ptr %arg, [ ptr edgeaddress(@foo, 0), label %preheader ptr edgeaddress(@foo, 1), label %stuff ]

  stuff:
    %0 = load i8, ptr %arg, align 1
    br label %loop

  return:
    ret void

}
