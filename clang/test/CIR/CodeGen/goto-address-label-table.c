// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefixes=LLVM,LLVMCIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefixes=LLVM,OGCG

// CIR-DAG: cir.global "private" internal dso_local @f.tbl = #cir.const_array<[#cir.block_addr_info<@f, "L1"> : !cir.ptr<!void>, #cir.block_addr_info<@f, "L2"> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 2>
// CIR-DAG: cir.global "private" internal dso_local @g.tbl = #cir.const_array<[#cir.block_addr_info<@g, "A"> : !cir.ptr<!void>, #cir.block_addr_info<@g, "A"> : !cir.ptr<!void>, #cir.block_addr_info<@g, "B"> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 3>
// CIR-DAG: cir.global "private" internal dso_local @h.tbl = #cir.const_array<[#cir.block_addr_info<@h, "L1"> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 1>
// CIR-DAG: cir.global "private" internal dso_local @m.ctbl = #cir.const_array<[#cir.block_addr_info<@m, "A2"> : !cir.ptr<!void>]> : !cir.array<!cir.ptr<!void> x 1>

// LLVMCIR-DAG: @f.tbl = internal global [2 x ptr] [ptr blockaddress(@f, %[[FL1:[0-9a-zA-Z_.]+]]), ptr blockaddress(@f, %[[FL2:[0-9a-zA-Z_.]+]])], align 16
// LLVMCIR-DAG: @g.tbl = internal global [3 x ptr] [ptr blockaddress(@g, %[[GA:[0-9a-zA-Z_.]+]]), ptr blockaddress(@g, %[[GA]]), ptr blockaddress(@g, %[[GB:[0-9a-zA-Z_.]+]])], align 16
// LLVMCIR-DAG: @h.tbl = internal global [1 x ptr] [ptr blockaddress(@h, %{{[0-9a-zA-Z_.]+}})], align 8
// LLVMCIR-DAG: @m.ctbl = internal global [1 x ptr] [ptr blockaddress(@m, %[[MA2:[0-9a-zA-Z_.]+]])], align 8
// OGCG-DAG: @f.tbl = internal global [2 x ptr] [ptr edgeaddress(@f, 0), ptr edgeaddress(@f, 1)], align 16
// OGCG-DAG: @g.tbl = internal global [3 x ptr] [ptr edgeaddress(@g, 0), ptr edgeaddress(@g, 0), ptr edgeaddress(@g, 1)], align 16
// OGCG-DAG: @h.tbl = internal global [1 x ptr] [ptr edgeaddress(@h, 0)], align 8
// OGCG-DAG: @m.ctbl = internal global [1 x ptr] [ptr edgeaddress(@m, 0)], align 8

int f(int x) {
  static const void *tbl[] = {&&L1, &&L2};
  goto *tbl[x];
L1:
  return 1;
L2:
  return 2;
}

// CIR-LABEL: cir.func {{.*}} @f
// CIR:   %[[TBL:.*]] = cir.get_global @f.tbl
// CIR:   cir.indirect_goto %{{.*}} : !cir.ptr<!void>
// CIR:   cir.label "L1"
// CIR:   cir.label "L2"

// LLVM-LABEL: define dso_local i32 @f(
// LLVMCIR:   indirectbr ptr %{{.*}}, [label %[[FL1]], label %[[FL2]]]
// OGCG:      indirectswitch ptr %{{.*}}, [
// OGCG-NEXT:   ptr edgeaddress(@f, 0), label %L1
// OGCG-NEXT:   ptr edgeaddress(@f, 1), label %L2
// OGCG-NEXT: ]

// A appears twice in g's table, but a block only needs to be listed once as an
// indirect-branch successor, so both CIR and classic codegen list it once.
int g(int x) {
  static const void *tbl[] = {&&A, &&A, &&B};
  goto *tbl[x];
A:
  return 1;
B:
  return 2;
}

// CIR-LABEL: cir.func {{.*}} @g
// CIR:   cir.indirect_goto %{{.*}} : !cir.ptr<!void>
// CIR:   cir.label "A"
// CIR:   cir.label "B"

// LLVM-LABEL: define dso_local i32 @g(
// LLVMCIR:   indirectbr ptr %{{.*}}, [label %[[GA]], label %[[GB]]]
// OGCG:      indirectswitch ptr %{{.*}}, [
// OGCG-NEXT:   ptr edgeaddress(@g, 0), label %A
// OGCG-NEXT:   ptr edgeaddress(@g, 1), label %B
// OGCG-NEXT: ]

// h takes a label address but never executes a `goto *`, so CIR emits no
// indirect goto (neither does classic codegen).
int h(int x) {
  static const void *tbl[] = {&&L1};
  (void)tbl;
  return x;
L1:
  return 0;
}

// CIR-LABEL: cir.func {{.*}} @h
// CIR-NOT: cir.indirect_goto

// LLVM-LABEL: define dso_local i32 @h(
// LLVMCIR-NOT: indirectbr
// OGCG-NOT: indirect
// OGCG: }

// A2's address comes from a constant table, B2's from a runtime block-address
// op; both feed the same indirect branch.
int m(int sel) {
  static const void *ctbl[] = {&&A2};
  void *p = &&B2;
  void *t = (void *)ctbl[0];
  void *dest = sel ? t : p;
  goto *dest;
A2:
  return 1;
B2:
  return 2;
}

// CIR-LABEL: cir.func {{.*}} @m
// CIR:   cir.block_address <@m, "B2">
// CIR:   cir.indirect_goto
// CIR-DAG:   cir.label "A2"
// CIR-DAG:   cir.label "B2"

// LLVM-LABEL: define dso_local i32 @m(
// LLVMCIR:   indirectbr ptr %{{.*}}, [label %[[MA2]], label %{{.+}}]
// OGCG:      store ptr edgeaddress(@m, 1), ptr %p
// OGCG:      indirectswitch ptr %{{.*}}, [
// OGCG-NEXT:   ptr edgeaddress(@m, 0), label %A2
// OGCG-NEXT:   ptr edgeaddress(@m, 1), label %B2
// OGCG-NEXT: ]
