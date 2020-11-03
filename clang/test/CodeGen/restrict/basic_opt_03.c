// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// XFAIL: *

// NOTE: SROA needs to be able to see through <tt>llvm.noalias</tt>. This is introduced in case of returning of larger structs.
struct A {
  int a, b, c, d, e, f, g, h;
};
struct A constructIt(int a) {
  struct A tmp = {a, a, a, a, a, a, a, a};
  return tmp;
}
int test_sroa01a(unsigned c) {
  int tmp = 0;
  for (int i = 0; i < c; ++i) {
    struct A a = constructIt(i);
    tmp = tmp + a.e;
  }
  return tmp;
}

// CHECK: @test_sroa01a
// CHECK: FIXME

int test_sroa01b(unsigned c) {
  int tmp = 0;
  for (int i = 0; i < c; ++i) {
    struct A a = {i, i, i, i, i, i, i, i};
    tmp = tmp + a.e;
  }
  return tmp;
}

// CHECK: @test_sroa01b
// CHECK: FIXME

int test_sroa01c(unsigned c) {
  int tmp = 0;
  for (int i = 0; i < c; ++i) {
    int *__restrict dummy; // should not influence optimizations !
    struct A a = {i, i, i, i, i, i, i, i};
    tmp = tmp + a.e;
  }
  return tmp;
}

// CHECK: @test_sroa01b
// CHECK: FIXME

int test_sroa02a(unsigned c) {
  int tmp = 0;
  struct A a;
  for (int i = 0; i < c; ++i) {
    a = constructIt(i);
    tmp = tmp + a.e;
  }
  return tmp;
}

// CHECK: @test_sroa02a
// CHECK: FIXME

int test_sroa02b(unsigned c) {
  struct A a;
  int tmp = 0;
  for (int i = 0; i < c; ++i) {
    a = (struct A){i, i, i, i, i, i, i, i};
    tmp = tmp + a.e;
  }
  return tmp;
}

// CHECK: @test_sroa02b
// CHECK: FIXME
