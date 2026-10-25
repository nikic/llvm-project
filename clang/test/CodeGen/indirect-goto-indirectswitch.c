// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,NOPTRAUTH
// RUN: %clang_cc1 -triple arm64e-apple-ios -fptrauth-indirect-gotos -emit-llvm -o - %s -DPTRAUTH | FileCheck %s --check-prefixes=CHECK,PTRAUTH
// RUN: %clang_cc1 -triple avr -emit-llvm -o - %s | FileCheck %s --check-prefix=AVR

// GNU labels as values are emitted as edgeaddress constants with per-function
// IDs (0..n-1, in the order the label addresses are first taken). Indirect
// gotos branch to a single block ending in an indirectswitch over all
// address-taken labels of the function.

// CHECK: @static_table.tbl = internal constant [3 x ptr] [ptr edgeaddress(@static_table, 0), ptr edgeaddress(@static_table, 1), ptr edgeaddress(@static_table, 0)]
// NOPTRAUTH: @label_diff.offs = internal constant [3 x i32] [i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@label_diff, 0) to i64), i64 ptrtoint (ptr edgeaddress(@label_diff, 0) to i64)) to i32), i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@label_diff, 1) to i64), i64 ptrtoint (ptr edgeaddress(@label_diff, 0) to i64)) to i32), i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@label_diff, 2) to i64), i64 ptrtoint (ptr edgeaddress(@label_diff, 0) to i64)) to i32)]
// CHECK: @no_indirect_goto.p = internal global ptr edgeaddress(@no_indirect_goto, 0)

// AVR: @static_table.tbl = internal constant [3 x ptr] [ptr addrspacecast (ptr addrspace(1) edgeaddress(@static_table, 0) to ptr), ptr addrspacecast (ptr addrspace(1) edgeaddress(@static_table, 1) to ptr), ptr addrspacecast (ptr addrspace(1) edgeaddress(@static_table, 0) to ptr)]

// CHECK-LABEL: define {{.*}}i32 @static_table(
// CHECK:         [[DEST:%.*]] = load ptr, ptr %arrayidx
// CHECK-NEXT:    br label %indirectgoto
// CHECK:       indirectgoto:
// CHECK-NEXT:    [[PHI:%.*]] = phi ptr [ [[DEST]], %entry ]
// CHECK-NEXT:    indirectswitch ptr [[PHI]], [
// CHECK-NEXT:      ptr edgeaddress(@static_table, 0), label %a
// CHECK-NEXT:      ptr edgeaddress(@static_table, 1), label %b
// CHECK-NEXT:    ]
// CHECK-NEXT:  }

// AVR-LABEL: define {{.*}} @static_table(
// AVR:         [[ADDR:%.*]] = addrspacecast ptr %{{.*}} to ptr addrspace(1)
// AVR:       indirectgoto:
// AVR-NEXT:    [[PHI:%.*]] = phi ptr addrspace(1) [ [[ADDR]], %entry ]
// AVR-NEXT:    indirectswitch ptr addrspace(1) [[PHI]], [
// AVR-NEXT:      ptr addrspace(1) edgeaddress(@static_table, 0), label %a
// AVR-NEXT:      ptr addrspace(1) edgeaddress(@static_table, 1), label %b
// AVR-NEXT:    ]
int static_table(int x) {
  static const void *const tbl[] = {&&a, &&b, &&a};
  goto *tbl[x];
a:
  return 1;
b:
  return 2;
}

// Multiple indirect gotos share the single indirectswitch; labels whose
// address is taken after the first goto are still cases of it.
// CHECK-LABEL: define {{.*}}i32 @multiple_gotos(
// CHECK:         store ptr edgeaddress(@multiple_gotos, 0), ptr %p
// CHECK:         select i1 %{{.*}}, ptr edgeaddress(@multiple_gotos, 1), ptr edgeaddress(@multiple_gotos, 2)
// CHECK:       indirectgoto:
// CHECK-NEXT:    phi ptr [ %{{.*}}, %entry ], [ %{{.*}}, %l1 ]
// CHECK-NEXT:    indirectswitch ptr %{{.*}}, [
// CHECK-NEXT:      ptr edgeaddress(@multiple_gotos, 0), label %l1
// CHECK-NEXT:      ptr edgeaddress(@multiple_gotos, 1), label %l2
// CHECK-NEXT:      ptr edgeaddress(@multiple_gotos, 2), label %l3
// CHECK-NEXT:    ]
// CHECK-NOT:     indirectswitch
// CHECK:       }

// The address of a label in an expression is an addrspacecast from the
// program address space.
// AVR-LABEL: define {{.*}} @multiple_gotos(
// AVR:         store ptr addrspacecast (ptr addrspace(1) edgeaddress(@multiple_gotos, 0) to ptr), ptr %p
// AVR:         select i1 %{{.*}}, ptr addrspacecast (ptr addrspace(1) edgeaddress(@multiple_gotos, 1) to ptr), ptr addrspacecast (ptr addrspace(1) edgeaddress(@multiple_gotos, 2) to ptr)
// AVR:       indirectgoto:
// AVR-NEXT:    phi ptr addrspace(1)
// AVR-NEXT:    indirectswitch ptr addrspace(1)
int multiple_gotos(int x) {
  void *p = &&l1;
  goto *p;
l1:
  p = x ? &&l2 : &&l3;
  goto *p;
l2:
  return 2;
l3:
  return 3;
}

// Label differences (offset tables used by interpreters) are constant
// expressions on edgeaddress. They are not supported with ptrauth indirect
// gotos.
// NOPTRAUTH-LABEL: define {{.*}}i32 @label_diff(
// NOPTRAUTH:         getelementptr i8, ptr edgeaddress(@label_diff, 0), i64 %
// NOPTRAUTH:         indirectswitch ptr %{{.*}}, [
// NOPTRAUTH-NEXT:      ptr edgeaddress(@label_diff, 0), label %a
// NOPTRAUTH-NEXT:      ptr edgeaddress(@label_diff, 1), label %b
// NOPTRAUTH-NEXT:      ptr edgeaddress(@label_diff, 2), label %c
// NOPTRAUTH-NEXT:    ]
#ifndef PTRAUTH
int label_diff(int x) {
  static const int offs[] = {&&a - &&a, &&b - &&a, &&c - &&a};
  goto *(&&a + offs[x]);
a:
  return 1;
b:
  return 2;
c:
  return 3;
}
#endif

// If the function has no indirect goto, no indirect goto block is created: the
// edgeaddresses are not cases of any indirectswitch (and have an unspecified
// value).
// CHECK-LABEL: define {{.*}}ptr @no_indirect_goto(
// CHECK:         store ptr edgeaddress(@no_indirect_goto, 1), ptr %q
// CHECK-NOT:     indirect
// CHECK:       }
void *no_indirect_goto(void) {
  static void *p = &&l1;
  void *q = &&l2;
l1:
l2:
  return q;
}

// An indirect goto to a constant label is a direct branch, so this does not
// need an indirectswitch either.
// CHECK-LABEL: define {{.*}}i32 @constant_target(
// CHECK:         br label %l
// CHECK-NOT:     indirect
// CHECK:       }
int constant_target(void) {
  goto *&&l;
l:
  return 0;
}

// PTRAUTH: attributes #{{[0-9]+}} = {{{.*}}"ptrauth-indirect-gotos"
