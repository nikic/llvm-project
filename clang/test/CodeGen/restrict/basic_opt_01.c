// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// Rough test to verify basic functionality of the 'restrict member pointer' rewrite

// CHECK: @test01A
// CHECK: ret i32 42
int test01A(int *pA, int *pB) {
  int *__restrict prA;
  prA = pA;

  *prA = 42;
  *pB = 43;
  return *prA;
}

// CHECK: @test01B
// CHECK: ret i32 42
int test01B(int *__restrict prA, int *pB) {
  *prA = 42;
  *pB = 43;
  return *prA;
}

// CHECK: @test02A
// CHECK: ret i32 42
int test02A(int b, int *pA, char *pB, int *pC) {
  int *__restrict prA;
  prA = pA;
  char *__restrict prB;
  prB = pB;
  char *lp = b ? (char *)prA : (char *)prB;

  *lp = 42;
  *pC = 43;
  return *lp;
}

// CHECK: @test02B
// CHECK: ret i32 42
int test02B(int b, int *__restrict prA, char *__restrict prB, int *pC) {
  char *lp = b ? (char *)prA : (char *)prB;

  *lp = 42;
  *pC = 43;
  return *lp;
}

// CHECK: @test03
// CHECK: ret i32 42
int test03(int n, int *pA, char *pB, int *pC) {
  do {
    int *__restrict prA;
    prA = pA;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *pA;
}

// CHECK: @test04A0
// CHECK: ret i32 42
int test04A0(int n, int *pA, char *pB, int *pC) {
  int *__restrict prA;
  do {
    prA = pA;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}

// CHECK: @test04A1
// CHECK: ret i32 42
int test04A1(int n, int *pA, char *pB, int *pC) {
  int *__restrict prA;
  prA = pA;
  do {
    prA = pA;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}

// CHECK: @test04B0
// CHECK: ret i32 42
int test04B0(int n, int *__restrict prA, char *pB, int *pC) {
  do {
    prA = prA;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}

// CHECK: @test04B1
// CHECK: ret i32 42
int test04B1(int n, int *__restrict prA, char *pB, int *pC) {
  prA = prA;
  do {
    prA = prA;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}

// CHECK: @test05A
// CHECK: ret i32 42
int test05A(int n, int *pA, char *pB, int *pC) {
  int *__restrict prA;
  prA = pA;
  do {
    prA++;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}

// CHECK: @test05B
// CHECK: ret i32 42
int test05B(int n, int *__restrict prA, char *pB, int *pC) {
  do {
    prA++;

    *prA = 42;
    *pC = 43;
  } while (n--);
  return *prA;
}
