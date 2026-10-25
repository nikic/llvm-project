; RUN: opt -S < %s -passes=jump-threading | FileCheck %s

; Keep edge addresses alive.
@addresses = constant [4 x ptr] [
  ptr edgeaddress(@test1, 0), ptr edgeaddress(@test1, 1),
  ptr edgeaddress(@test2, 0), ptr edgeaddress(@test2, 1)
]

declare void @bar()
declare void @baz()



; Check basic jump threading for indirectswitch instructions.

; CHECK: void @test1
; CHECK: br i1 %tobool, label %L1, label %indirectgoto
; CHECK-NOT: if.else:
; CHECK: L1:
; CHECK: indirectswitch ptr %address, [
; CHECK-NEXT: ptr edgeaddress(@test1, 0), label %L1
; CHECK-NEXT: ptr edgeaddress(@test1, 1), label %L2
define void @test1(i32 %i, ptr %address) nounwind {
entry:
  %rem = srem i32 %i, 2
  %tobool = icmp ne i32 %rem, 0
  br i1 %tobool, label %indirectgoto, label %if.else

if.else:                                          ; preds = %entry
  br label %indirectgoto

L1:                                               ; preds = %indirectgoto
  call void @bar()
  ret void

L2:                                               ; preds = %indirectgoto
  call void @baz()
  ret void

indirectgoto:                                     ; preds = %if.else, %entry
  %indirect.goto.dest = phi ptr [ %address, %if.else ], [ edgeaddress(@test1, 0), %entry ]
  indirectswitch ptr %indirect.goto.dest, [ ptr edgeaddress(@test1, 0), label %L1 ptr edgeaddress(@test1, 1), label %L2 ]
}


; Check constant folding of indirectswitch

; CHECK: void @test2
; CHECK-NEXT: L1:
; CHECK-NEXT: call void @bar
; CHECK-NEXT: ret void
define void @test2() nounwind {
entry:
  indirectswitch ptr edgeaddress(@test2, 0), [ ptr edgeaddress(@test2, 0), label %L1 ptr edgeaddress(@test2, 1), label %L2 ]

L1:                                               ; preds = %indirectgoto
  call void @bar()
  ret void

L2:                                               ; preds = %indirectgoto
  call void @baz()
  ret void
}


; PR4151
; A blockaddress is data-only, so address-taken blocks are merged (except into
; the entry block), and the blockaddresses then refer to the merged block.
@.str = private unnamed_addr constant [4 x i8] c"%p\0A\00"

; CHECK-LABEL: @test3(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %__here3
; CHECK:       __here3:
; CHECK-NEXT:    call {{.*}} blockaddress(@test3, %__here3)
; CHECK-NEXT:    call {{.*}} blockaddress(@test3, %__here3)
; CHECK-NEXT:    call {{.*}} blockaddress(@test3, %__here3)
; CHECK-NEXT:    ret void
define void @test3() nounwind ssp noredzone {
entry:
  br label %__here

__here:                                           ; preds = %entry
  %call = call i32 (...) @printf(ptr @.str, i64 ptrtoint (ptr blockaddress(@test3, %__here) to i64)) nounwind noredzone
  br label %__here1

__here1:                                          ; preds = %__here
  %call2 = call i32 (...) @printf(ptr @.str, i64 ptrtoint (ptr blockaddress(@test3, %__here1) to i64)) nounwind noredzone
  br label %__here3

__here3:                                          ; preds = %__here1
  %call4 = call i32 (...) @printf(ptr @.str, i64 ptrtoint (ptr blockaddress(@test3, %__here3) to i64)) nounwind noredzone
  ret void
}

declare i32 @printf(...) noredzone
