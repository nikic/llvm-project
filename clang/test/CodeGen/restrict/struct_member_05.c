// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// restrict member pointers and inlining - basic functionality test

struct FOO {
  int *__restrict pA;
  int *__restrict pB;
};

struct FOO_plain {
  int *pA;
  int *pB;
};

int test10(int *pA, int *pB, int *pC) {

  *pA = 40;
  {
    struct FOO rp;
    rp.pA = pA;
    rp.pB = pB;

    {
      struct FOO *p = &rp;
      *p->pA = 42;
      *p->pB = 43;
    }

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

    {
      *rp.pA = 42;
      *rp.pB = 43;
    }

    *pC = 99;
    return *rp.pA; // 42
  }
}

// CHECK-LABEL: @test11(
// CHECK: ret i32 42

int test12a(int *pA, int *pB, int *pC, struct FOO *pF) {

  *pA = 40;
  {
    struct FOO rp;
    rp.pA = pA;
    rp.pB = pB;

    {
      struct FOO *p = pF ? pF : &rp;
      *p->pA = 42;
      *p->pB = 43;
    }

    *pC = 99;
    return *rp.pA; // 42 or 40
  }
}

// CHECK-LABEL: @test12a(
// CHECK-NOT: ret i32 40
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99

int test12b(int *pA, int *pB, int *pC, struct FOO_plain *pF) {

  *pA = 40;
  {
    struct FOO_plain rp;
    rp.pA = pA;
    rp.pB = pB;

    {
      struct FOO_plain *p = pF ? pF : &rp;
      *p->pA = 42;
      *p->pB = 43;
    }

    *pC = 99;
    return *rp.pA; // 42 or 40 or 99 or ...
  }
}

// CHECK-LABEL: @test12b(
// CHECK-NOT: ret i32 40
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99
