// RUN: %clang_cc1 %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 %s -emit-llvm -o - -fexperimental-new-constant-interpreter | FileCheck %s
// REQUIRES: asserts

// CHECK: @a.a = internal global ptr edgeaddress(@a, 0)
void a(void) {
A:;static void* a = &&A;
}

// PR14005
// CHECK: @b.ar = internal global {{.*}} sub (i{{..}} ptrtoint (ptr edgeaddress(@b, 0) to i{{..}}), i{{..}} ptrtoint (ptr edgeaddress(@b, 1) to i{{..}}))
int b(void) {
  static int ar = &&l2 - &&l1;
l1:
  return 10;
l2:
  return 11;
}
