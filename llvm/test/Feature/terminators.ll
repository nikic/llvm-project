; RUN: llvm-as < %s | llvm-dis > %t1.ll
; RUN: llvm-as %t1.ll -o - | llvm-dis > %t2.ll
; RUN: diff %t1.ll %t2.ll

        %int = type i32

define i32 @squared(i32 %i0) {
        switch i32 %i0, label %Default [
                 i32 1, label %Case1
                 i32 2, label %Case2
                 i32 4, label %Case4
        ]

Default:                ; preds = %0
        ret i32 -1

Case1:          ; preds = %0
        ret i32 1

Case2:          ; preds = %0
        ret i32 4

Case4:          ; preds = %0
        ret i32 16
}


@Addr = global ptr edgeaddress(@indbrtest, 0)
@Addr3 = global ptr blockaddress(@squared, %Case1)


define i32 @indbrtest(ptr %P, ptr %Q) {
  br label %dispatch
BB1:
  indirectswitch ptr %Q, []
BB2:
  %R = bitcast ptr edgeaddress(@indbrtest, 2) to ptr
  br label %dispatch
BB3:
  ret i32 2

dispatch:
  %dispatch.addr = phi ptr [ %P, %0 ], [ %R, %BB2 ]
  indirectswitch ptr %dispatch.addr, [
    ptr edgeaddress(@indbrtest, 0), label %BB1
    ptr edgeaddress(@indbrtest, 1), label %BB2
    ptr edgeaddress(@indbrtest, 2), label %BB3
  ]
}


