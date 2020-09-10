// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

struct FOO {
  int *__restrict pA;
};

struct BAR {
  int *pA;
};

static void adaptFOO(struct FOO *p) {
  *p->pA = 99;
};

static void adaptBAR(struct BAR *p) {
  *p->pA = 99;
};

static void adaptInt(int *p) {
  *p = 99;
};

// has 'unknown scope': caller: no, callee: yes
int test10(int *pA, struct FOO *pB) {
  *pA = 42;
  adaptFOO(pB);
  return *pA;
}
// CHECK-LABEL: @test10(
// CHECK: ret i32 42

// has 'unknown scope': caller: yes, callee: no
int test11(int *pA, struct FOO *pB) {
  *pB->pA = 42;
  adaptInt(pA);
  return *pB->pA;
}
// CHECK-LABEL: @test11(
// CHECK: ret i32 42

// has 'unknown scope': caller: no, callee: no
int test12(int *pA, struct BAR *pB) {
  *pA = 42;
  adaptBAR(pB);
  return *pA;
}
// CHECK-LABEL: @test12(
// CHECK-NOT: ret i32 42

// has 'unknown scope': caller: yes, callee: yes
int test13(int *pA, struct FOO *pB) {
  *pB->pA = 41; // introduce 'unknown scope'
  *pA = 42;
  adaptFOO(pB);
  return *pA;
}

// CHECK-LABEL: @test13(
// CHECK: ret i32 42
