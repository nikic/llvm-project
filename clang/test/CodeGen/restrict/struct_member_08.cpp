// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s --check-prefixes=CHECK,CHECK64
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s --check-prefixes=CHECK,CHECK32

// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_BEFORE | FileCheck %s --check-prefixes=CHECK,CHECK64
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_BEFORE | FileCheck %s --check-prefixes=CHECK,CHECK32

// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_AFTER | FileCheck %s --check-prefixes=CHECK,CHECK64
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_AFTER | FileCheck %s --check-prefixes=CHECK,CHECK32

// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_BEFORE -DDUMMY_AFTER | FileCheck %s --check-prefixes=CHECK,CHECK64
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - -DDUMMY_BEFORE -DDUMMY_AFTER | FileCheck %s --check-prefixes=CHECK,CHECK32

// NOTE: this test in C++ mode

struct Fum {
  Fum(unsigned long long d) {
    ptr1 = ((int *)(d & 0xffffffff));
    ptr2 = ((int *)((d >> 32) & 0xffffffff));
  }
  Fum(const Fum &) = default;

#ifdef DUMMY_BEFORE
  int *dummyb0;
  int *dummyb1;
#endif
  int *__restrict ptr1;
  int *__restrict ptr2;
#ifdef DUMMY_AFTER
  int *dummya0;
  int *dummya1;
#endif
};

static Fum pass(Fum d) { return d; }

int test_Fum_01(unsigned long long data, int *p1) {
  Fum tmp = {data};

  int *p0 = tmp.ptr1;

  *p0 = 42;
  *p1 = 99;
  return *p0;
}
// CHECK-LABEL: @_Z11test_Fum_01yPi
// CHECK-NOT: alloca
// CHECK: ret i32 42

int test_Fum_02(unsigned long long data) {
  Fum tmp = {data};

  int *p0 = tmp.ptr1;
  int *p1 = tmp.ptr2;

  *p0 = 42;
  *p1 = 99;
  return *p0;
}
// CHECK-LABEL: @_Z11test_Fum_02y
// CHECK-NOT: alloca
// CHECK: ret i32 42

int test_Fum_pass_01(unsigned long long data, int *p1) {
  Fum tmp = {data};

  int *p0 = pass(tmp).ptr1;

  *p0 = 42;
  *p1 = 99;
  return *p0;
}
// CHECK-LABEL: @_Z16test_Fum_pass_01yPi
// CHECK-NOT: alloca
// CHECK: ret i32 42

int test_Fum_pass_02(unsigned long long data) {
  Fum tmp = {data};

  int *p0 = pass(tmp).ptr1;
  int *p1 = pass(tmp).ptr2;

  *p0 = 42;
  *p1 = 99;
  return *p0;
}
// CHECK-LABEL: @_Z16test_Fum_pass_02y
// CHECK-NOT: alloca
// CHECK: ret i32 42

int test_Fum_pass_03(unsigned long long data) {
  Fum tmp = {data};

  int *b0 = tmp.ptr1;
  *b0 = 42;

  int *p0 = pass(tmp).ptr1;

  *p0 = 99;
  return *b0; // 99
}
// CHECK-LABEL: @_Z16test_Fum_pass_03y
// CHECK-NOT: alloca
// CHECK-NOT: ret i32 42
// CHECK: ret i32 99

int test_Fum_pass_04(unsigned long long data, int *px) {
  Fum tmp = {data};

  int *b0 = tmp.ptr1;
  *b0 = 42;
  tmp.ptr1 = px;

  int *p0 = pass(tmp).ptr1;

  *p0 = 99;
  return *b0; // 42 or 99
}
// CHECK-LABEL: @_Z16test_Fum_pass_04yPi
// CHECK-NOT: alloca
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 99
// CHECK: ret i32 %

class S {
public:
  S(int *d) : data(d) {}
  int *getData() { return data; }

private:
  int *__restrict__ data;
};

int test_S__01(int *pA, long N) {
  int *__restrict__ x = pA;

  *x = 42;
  {
    S s(x + N);
    *s.getData() = 99;
  }
  return *x; // N could be 0
}

// CHECK-LABEL: @_Z10test_S__01Pil
// CHECK-NOT: alloca
// CHECK-NOT: ret i32 42
// CHECK: ret i32 %

int test_S__02(int *pA, long N) {
  int *__restrict__ x = pA;

  *x = 42;
  {
    S s(x + N);
    *s.getData() = 99;
    return *x; // restrict rules say that N cannot be 0
  }
}

// CHECK-LABEL: @_Z10test_S__02Pil
// CHECK-NOT: alloca
// CHECK: ret i32 42
