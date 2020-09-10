// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

struct FOO {
  int *__restrict pA;
  int *__restrict pB;
};

int test10(int *pA, int *pB) {
  struct FOO rp;
  rp.pA = pA;
  rp.pB = pB;

  *rp.pA = 42;
  *rp.pB = 99;

  return *rp.pA; //42
}
// CHECK-LABEL: @test10(
// CHECK: ret i32 42

int test11(struct FOO rp) {
  *rp.pA = 42;
  *rp.pB = 99;

  return *rp.pA; //42
}
// CHECK-LABEL: @test11(
// CHECK: ret i32 42

int test12(struct FOO *rp) {
  *rp->pA = 42;
  *rp->pB = 99;

  return *rp->pA; //42
}
// CHECK-LABEL: @test12(
// CHECK: ret i32 42

int test20(int *pA, int *pB) {
  struct FOO rp0;
  struct FOO rp1;
  rp0.pB = pA;
  rp1.pB = pB;

  *rp0.pB = 42;
  *rp1.pB = 99;

  return *rp0.pB; //42
}
// CHECK-LABEL: @test20(
// CHECK: ret i32 42

int test21(struct FOO rp0, struct FOO rp1) {
  *rp0.pB = 42;
  *rp1.pB = 99;

  return *rp0.pB; //42
}
// CHECK-LABEL: @test21(
// CHECK: ret i32 42

int test22(struct FOO *rp0, struct FOO *rp1) {
  *rp0->pB = 42;
  *rp1->pB = 99;

  return *rp0->pB; // needs load
}
// CHECK-LABEL: @test22(
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 99

int test23(struct FOO *rp0, struct FOO *rp1) {
  *rp0->pA = 42;
  *rp1->pB = 99;

  return *rp0->pA; // 42, rp0->pA and rp1->pB are not overlapping
}
// CHECK-LABEL: @test23(
// CHECK: ret i32 42

int test24(struct FOO *__restrict rp0, struct FOO *rp1) {
  *rp0->pB = 42;
  *rp1->pB = 99;

  return *rp0->pB; // 42
}
// CHECK-LABEL: @test24(
// CHECK: ret i32 42

int test25(struct FOO *p0, struct FOO *rp1) {
  struct FOO *__restrict rp0;
  rp0 = p0;
  *rp0->pB = 42;
  *rp1->pB = 99;

  return *rp0->pB; // 42
}
// CHECK-LABEL: @test25(
// CHECK: ret i32 42
