; RUN: mlir-translate --import-llvm %s | FileCheck %s
; RUN: mlir-translate --import-llvm %s | mlir-translate -mlir-to-llvmir | FileCheck %s --check-prefix=ROUNDTRIP

; An indirectswitch is imported as an llvm.indirectbr over its (unique) case
; destinations. Each case destination gets a block tag with the edge address
; ID, edge addresses become block addresses of these tags.

; CHECK: llvm.mlir.global external constant @tbl()
; CHECK:   llvm.blockaddress <function = @f, tag = <id = 2>> : !llvm.ptr
; CHECK:   llvm.blockaddress <function = @f, tag = <id = 1>> : !llvm.ptr
; CHECK:   llvm.blockaddress <function = @f, tag = <id = 0>> : !llvm.ptr
; ROUNDTRIP: @tbl = constant [3 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1), ptr edgeaddress(@f, 2)]
@tbl = constant [3 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1), ptr edgeaddress(@f, 2)]

; An edge address that is not a case of an indirectswitch has an unspecified
; value. It is imported as the address of the function.
; CHECK: llvm.mlir.global external constant @unused()
; CHECK:   llvm.mlir.addressof @f : !llvm.ptr
; ROUNDTRIP: @unused = constant ptr @f
@unused = constant ptr edgeaddress(@f, 7)

; CHECK: llvm.func @f(%[[ARG:.*]]: i32) -> i32
define i32 @f(i32 %i) {
entry:
  %p = getelementptr [3 x ptr], ptr @tbl, i32 0, i32 %i
  %t = load ptr, ptr %p
  br label %ig
ig:
  ; CHECK: ^[[IG:.*]](%[[ADDR:.*]]: !llvm.ptr, %[[V:.*]]: i32):
  ; CHECK:   llvm.indirectbr %[[ADDR]] : !llvm.ptr, [
  ; CHECK-NEXT: ^[[A:[^,]*]],
  ; CHECK-NEXT: ^[[B:.*]](%[[V]] : i32)
  ; CHECK-NEXT: ]
  ; ROUNDTRIP: indirectswitch ptr %{{.*}}, [
  ; ROUNDTRIP-NEXT: ptr edgeaddress(@f, 0), label %[[A:.*]]
  ; ROUNDTRIP-NEXT: ptr edgeaddress(@f, 1), label %[[B:.*]]
  ; ROUNDTRIP-NEXT: ptr edgeaddress(@f, 2), label %[[B]]
  ; ROUNDTRIP-NEXT: ]
  %addr = phi ptr [%t, %entry], [edgeaddress(@f, 1), %a]
  %v = phi i32 [0, %entry], [5, %a]
  indirectswitch ptr %addr, [ ptr edgeaddress(@f, 0), label %a
                              ptr edgeaddress(@f, 1), label %b
                              ptr edgeaddress(@f, 2), label %b ]
a:
  ; CHECK: ^[[A]]:
  ; CHECK-NEXT: llvm.blocktag <id = 0>
  ; CHECK-NEXT: llvm.br ^[[IG]]
  br label %ig
b:
  ; CHECK: ^[[B]](%[[R:.*]]: i32):
  ; CHECK-NEXT: llvm.blocktag <id = 1>
  ; CHECK-NEXT: llvm.blocktag <id = 2>
  ; CHECK-NEXT: llvm.return %[[R]] : i32
  ; ROUNDTRIP: [[B]]:
  ; ROUNDTRIP-NEXT: phi i32 [ %[[V:.*]], %[[IG:.*]] ], [ %[[V]], %[[IG]] ]
  %r = phi i32 [%v, %ig], [%v, %ig]
  ret i32 %r
}

; Block address tags start after the largest edge address ID of the function.

; CHECK-LABEL: llvm.func @mixed
define ptr @mixed(ptr %p) {
entry:
  ; CHECK: llvm.blockaddress <function = @mixed, tag = <id = [[BA_ID:.*]]>>
  ; CHECK: llvm.indirectbr
  ; ROUNDTRIP-LABEL: define ptr @mixed(
  ; ROUNDTRIP: indirectswitch ptr %{{.*}}, [
  ; ROUNDTRIP-NEXT: ptr edgeaddress(@mixed, 3), label %[[A:.*]]
  ; ROUNDTRIP-NEXT: ]
  indirectswitch ptr %p, [ ptr edgeaddress(@mixed, 3), label %a ]
a:
  ; CHECK: llvm.blocktag <id = 3>
  br label %b
b:
  ; CHECK: llvm.blocktag <id = [[BA_ID]]>
  ; ROUNDTRIP: ret ptr blockaddress(@mixed, %{{.*}})
  ret ptr blockaddress(@mixed, %b)
}
