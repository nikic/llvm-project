// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// restrict member pointers and inlining - basic functionality test

struct FOO {
  int *__restrict pA;
  int *__restrict pB;
};

void setFOO(struct FOO *p) {
  *p->pA = 42;
  *p->pB = 43;
}

int test10(int *pA, int *pB, int *pC) {

  *pA = 40;
  {
    struct FOO rp;
    rp.pA = pA;
    rp.pB = pB;

    setFOO(&rp);

    *pC = 99;
    return *rp.pA; // 42
  }
}

// CHECK-LABEL: @test10(
// CHECK: ret i32 42

int test11(int *pA, int *pB, int *pC) {

  *pA = 40;
  {
    struct FOO rp;
    rp.pA = pA;
    rp.pB = pB;

    setFOO(&rp);

    *pC = 99;
  }
  return *pA; // 42 // should be, but llvm does not see it
}

// CHECK-LABEL: @test11(
// CHECK-NOT: ret i32 40
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99

int test12(int *pA, int *pB, int *pC) {

  *pA = 40;
  {
    struct FOO rp;
    rp.pA = pA;
    rp.pB = pB;

    setFOO(&rp);
  }

  *pC = 99;
  return *pA; // 42 or 99
}

// CHECK-LABEL: @test12(
// CHECK-NOT: ret i32 40
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99

// out-of-function scope
int getFOO(struct FOO *p) {
  return *p->pA;
}

// fully defined
int test20(int *pA, int *pC) {
  *pA = 42;
  {
    struct FOO rp;
    rp.pA = pA;
    *pC = 99;
    return getFOO(&rp);
  }
}
// CHECK-LABEL: @test20(
// CHECK: ret i32 42

// fully defined
int test21(int *pA, int *pC) {
  *pA = 42;
  *pC = 99;
  {
    struct FOO rp;
    rp.pA = pA;
    return getFOO(&rp);
  }
}

// CHECK-LABEL: @test21(
// CHECK: ret i32 %

// mixed defined
int test22(int *pA, struct FOO *pB0, int b0, int *pC) {
  *pA = 42;
  {
    struct FOO rp;
    rp.pA = pA;
    *pC = 99;
    return getFOO(b0 ? &rp : pB0);
  }
}
// CHECK-LABEL: @test22(
// CHECK: ret i32 %

// mixed-mixed defined
int test23(int *pA, struct FOO *pB0, int b0, struct FOO *pB1, int b1, int *pC) {
  *pA = 41;
  {
    struct FOO rp;
    rp.pA = pA;
    *pC = 98;
    return test22(pA, b1 ? &rp : pB0, b0, pC);
  }
}
// CHECK-LABEL: @test23(
// CHECK: ret i32 %

// fully defined
int test24(int *pA, int *pB0, int b0, int *pB1, int b1, int *pC) {
  *pA = 40;
  {
    struct FOO fb0;
    fb0.pA = pB0;
    {
      struct FOO fb1;
      fb1.pA = pB1;

      return test23(pA, &fb0, b0, &fb1, b1, pC);
    }
  }
}

// CHECK-LABEL: @test24(
// CHECK: ret i32 %

int test25(int *pA, int b0, int b1, int *pC) {
  *pA = 40;
  {
    struct FOO fb0;
    fb0.pA = pA;
    {
      struct FOO fb1;
      fb1.pA = pA;

      return test23(pA, &fb0, b0, &fb1, b1, pC);
    }
  }
}

// CHECK-LABEL: @test25(
// FIXME: should be:  ret i32 42
// CHECK: ret i32 %
