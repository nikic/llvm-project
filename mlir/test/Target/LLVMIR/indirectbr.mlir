// RUN: mlir-translate -mlir-to-llvmir %s -split-input-file | FileCheck %s

// llvm.indirectbr is translated to an LLVM indirectswitch and the addresses of
// the tagged blocks it can jump to are translated to edge addresses whose ID is
// the block tag ID. All llvm.indirectbr operations of a function are merged
// into a single dispatch block, since an edge address cannot be a case of more
// than one indirectswitch.

llvm.func @callee(!llvm.ptr, i32, i32) -> i32

// CHECK: define i32 @test_indirectbr_phi(ptr %[[IN_PTR:.*]], ptr %[[ARG1:.*]], i32 %[[ARG2:.*]]) {
llvm.func @test_indirectbr_phi(%arg0: !llvm.ptr, %arg1: !llvm.ptr, %arg3: i32) -> i32 {
  %0 = llvm.mlir.undef : i1
  %2 = llvm.blockaddress <function = @test_indirectbr_phi, tag = <id = 1>> : !llvm.ptr
  %3 = llvm.mlir.constant(1 : i32) : i32
  %4 = llvm.mlir.constant(2 : i32) : i32
  %5 = llvm.select %0, %2, %arg0 : i1, !llvm.ptr
  // CHECK:   %[[BA0:.*]] = select i1 undef, ptr edgeaddress(@test_indirectbr_phi, 1), ptr %[[IN_PTR]]
  // CHECK:   br label %[[DISPATCH:.*]]
  llvm.indirectbr %5 : !llvm.ptr, [
  ^bb1,
  ^bb2(%3 : i32)
  ]
^bb1:
  // CHECK: [[BB1:.*]]:
  // CHECK:   %[[BA1:.*]] = select i1 undef, ptr edgeaddress(@test_indirectbr_phi, 1), ptr %[[IN_PTR]]
  // CHECK:   br label %[[DISPATCH]]
  llvm.blocktag <id = 2>
  %6 = llvm.select %0, %2, %arg0 : i1, !llvm.ptr
  llvm.indirectbr %6 : !llvm.ptr, [
  ^bb2(%4 : i32),
  ^bb3
  ]
^bb2(%7: i32):
  // CHECK: [[BB2:.*]]:
  // CHECK:   %[[PHI:.*]] = phi i32 [ %[[DISPATCH_ARG:.*]], %[[DISPATCH]] ]
  // CHECK:   %[[CALL:.*]] = call i32 @callee(ptr %[[ARG1]], i32 %[[ARG2]], i32 %[[PHI]])
  // CHECK:   ret i32 %[[CALL]]
  llvm.blocktag <id = 0>
  %8 = llvm.call @callee(%arg1, %arg3, %7) : (!llvm.ptr, i32, i32) -> i32
  llvm.return %8 : i32
^bb3:
  // CHECK: [[RET_BB:.*]]:
  // CHECK:   ret i32 %[[ARG2]]
  llvm.blocktag <id = 1>
  llvm.return %arg3 : i32
  // CHECK: [[DISPATCH]]:
  // CHECK:   %[[ADDR:.*]] = phi ptr [ %[[BA0]], %{{.*}} ], [ %[[BA1]], %[[BB1]] ]
  // CHECK:   %[[DISPATCH_ARG]] = phi i32 [ 1, %{{.*}} ], [ 2, %[[BB1]] ]
  // CHECK:   indirectswitch ptr %[[ADDR]], [
  // CHECK-NEXT: ptr edgeaddress(@test_indirectbr_phi, 0), label %[[BB2]]
  // CHECK-NEXT: ptr edgeaddress(@test_indirectbr_phi, 1), label %[[RET_BB]]
  // CHECK-NEXT: ptr edgeaddress(@test_indirectbr_phi, 2), label %[[BB1]]
  // CHECK-NEXT: ]
  // CHECK: }
}

// -----

// A single llvm.indirectbr is translated in place. Untagged successors cannot
// be reached and get no case; a block with several tags gets one case per tag.

// CHECK-LABEL: define i32 @single(
// CHECK-SAME: i32 %[[ARG:.*]])
llvm.func @single(%arg0: !llvm.ptr, %arg1: i32) -> i32 {
  // CHECK:   indirectswitch ptr %{{.*}}, [
  // CHECK-NEXT: ptr edgeaddress(@single, 3), label %[[A:.*]]
  // CHECK-NEXT: ptr edgeaddress(@single, 5), label %[[B:.*]]
  // CHECK-NEXT: ptr edgeaddress(@single, 7), label %[[A]]
  // CHECK-NEXT: ]
  llvm.indirectbr %arg0 : !llvm.ptr, [
  ^bb1(%arg1 : i32),
  ^bb2,
  ^bb3
  ]
^bb1(%0: i32):
  // CHECK: [[A]]:
  // CHECK:   phi i32 [ %[[ARG]], %[[ENTRY:.*]] ], [ %[[ARG]], %[[ENTRY]] ]
  llvm.blocktag <id = 7>
  llvm.blocktag <id = 3>
  llvm.return %0 : i32
^bb2:
  // CHECK: [[B]]:
  llvm.blocktag <id = 5>
  llvm.return %arg1 : i32
^bb3:
  // CHECK: :
  // CHECK-SAME: No predecessors!
  %1 = llvm.mlir.constant(0 : i32) : i32
  llvm.return %1 : i32
}

// -----

// Block addresses of blocks that are not a successor of an llvm.indirectbr stay
// LLVM block addresses. Block addresses in global initializers are translated
// to edge addresses, too.

// CHECK: @tbl = private constant [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)]
llvm.mlir.global private constant @tbl() : !llvm.array<2 x ptr> {
  %0 = llvm.blockaddress <function = @f, tag = <id = 0>> : !llvm.ptr
  %1 = llvm.blockaddress <function = @f, tag = <id = 1>> : !llvm.ptr
  %2 = llvm.mlir.undef : !llvm.array<2 x ptr>
  %3 = llvm.insertvalue %0, %2[0] : !llvm.array<2 x ptr>
  %4 = llvm.insertvalue %1, %3[1] : !llvm.array<2 x ptr>
  llvm.return %4 : !llvm.array<2 x ptr>
}

// CHECK: @data = private constant ptr blockaddress(@f, %[[DATA_BB:.*]])
llvm.mlir.global private constant @data() : !llvm.ptr {
  %0 = llvm.blockaddress <function = @f, tag = <id = 2>> : !llvm.ptr
  llvm.return %0 : !llvm.ptr
}

// CHECK: define void @f(
llvm.func @f(%arg0: i32) {
  %0 = llvm.mlir.addressof @tbl : !llvm.ptr
  %1 = llvm.getelementptr %0[0, %arg0] : (!llvm.ptr, i32) -> !llvm.ptr, !llvm.array<2 x ptr>
  %2 = llvm.load %1 : !llvm.ptr -> !llvm.ptr
  // CHECK:   indirectswitch ptr %{{.*}}, [
  // CHECK-NEXT: ptr edgeaddress(@f, 0), label %[[BB0:.*]]
  // CHECK-NEXT: ptr edgeaddress(@f, 1), label %[[BB1:.*]]
  // CHECK-NEXT: ]
  llvm.indirectbr %2 : !llvm.ptr, [
  ^bb1,
  ^bb2
  ]
^bb1:
  llvm.blocktag <id = 0>
  llvm.br ^bb3
^bb2:
  llvm.blocktag <id = 1>
  llvm.br ^bb3
^bb3:
  // CHECK: [[DATA_BB]]:
  llvm.blocktag <id = 2>
  llvm.return
}

// -----

// No llvm.indirectbr in the function: block addresses stay block addresses.

// CHECK-LABEL: define ptr @no_indirectbr(
llvm.func @no_indirectbr() -> !llvm.ptr {
  // CHECK: ret ptr blockaddress(@no_indirectbr, %1)
  %0 = llvm.blockaddress <function = @no_indirectbr, tag = <id = 0>> : !llvm.ptr
  llvm.br ^bb1
^bb1:
  llvm.blocktag <id = 0>
  llvm.return %0 : !llvm.ptr
}
