// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

struct FOO {
  int *__restrict pA;
  int *__restrict pB;
};

int test00a(int *pA, int *pB) {
  int *__restrict rpA;
  int *__restrict rpB;
  rpA = pA;
  rpB = pB;

  *rpA = 42;
  *rpB = 43;
  return *rpA;
}

// CHECK-LABEL: @test00a(
// CHECK: ret i32 42

int test00b(int *pA, int *pB) {
  int *__restrict rp[2];
  rp[0] = pA;
  rp[1] = pB;

  *rp[0] = 42;
  *rp[1] = 43;
  return *rp[0];
}

// CHECK-LABEL: @test00b(
// CHECK: ret i32 42

int test01(struct FOO *p0, struct FOO *p1) {
  *p0->pA = 42;
  *p1->pA = 43;

  return *p0->pA; // 42 or 43
}
// CHECK-LABEL: @test01(
// CHECK-NOT: ret i32 42

int test11(struct FOO *p0, struct FOO *p1) {
  *p0->pA = 42;
  *p1->pB = 43;

  return *p0->pA; // 42
}

// CHECK-LABEL: @test11(
// CHECK: ret i32 42

int test21(struct FOO p0, struct FOO p1) {
  *p0.pA = 42;
  *p1.pB = 43;

  return *p0.pA; // 42
}

// CHECK-LABEL: @test21(
// CHECK: ret i32 42

int test31(struct FOO *p0, struct FOO *__restrict p1) {
  *p0->pA = 42;
  *p1->pA = 43;

  return *p0->pA; // 42
}

// CHECK-LABEL: @test31(
// CHECK: ret i32 42
