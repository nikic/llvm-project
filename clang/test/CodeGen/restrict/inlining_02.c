// sfg-check: check resulting chains
// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// Base test for restrict propagation with inlining

//#define INLINE __attribute__((alwaysinline))
//#define INLINE inline
#define INLINE

void set_nnn(int *pA, int *pB, int *dummy) {
  int *lpA;
  lpA = pA;
  int *lpB;
  lpB = pB;
  int *ldummy;
  ldummy = dummy;
  *lpA = 42;
  *lpB = 43;

  *ldummy = 99;
}
// CHECK-LABEL: @set_nnn(

void set_nna(int *pA, int *pB, int *__restrict dummy) {
  int *lpA;
  lpA = pA;
  int *lpB;
  lpB = pB;
  int *ldummy;
  ldummy = dummy;
  *lpA = 42;
  *lpB = 43;

  *ldummy = 99;
}
// CHECK-LABEL: @set_nna(

void set_nnr(int *pA, int *pB, int *dummy) {
  int *lpA;
  lpA = pA;
  int *lpB;
  lpB = pB;
  int *__restrict ldummy;
  ldummy = dummy;

  *lpA = 42;
  *lpB = 43;

  *ldummy = 99;
}
// CHECK-LABEL: @set_nnr(

int test_rr_nnn(int *pA, int *pB, int *dummy) {
  int *__restrict lpA;
  lpA = pA;
  int *__restrict lpB;
  lpB = pB;

  set_nnn(lpA, lpB, dummy);
  return *lpA;
}

// CHECK-LABEL: @test_rr_nnn(
// CHECK: ret i32 42

int test_rr_nna(int *pA, int *pB, int *dummy) {
  int *__restrict lpA;
  lpA = pA;
  int *__restrict lpB;
  lpB = pB;

  set_nna(lpA, lpB, dummy);
  return *lpA;
}

// CHECK-LABEL: @test_rr_nna(
// CHECK: ret i32 42

int test_rr_nnr(int *pA, int *pB, int *dummy) {
  int *__restrict lpA;
  lpA = pA;
  int *__restrict lpB;
  lpB = pB;

  set_nnr(lpA, lpB, dummy);
  return *lpA;
}

// CHECK-LABEL: @test_rr_nnr(
// CHECK: ret i32 42

// -----------------------------------------------------------

int test_rr_local_nnn(int *pA, int *pB, int *dummy) {
  int *__restrict lpA;
  lpA = pA;
  int *__restrict lpB;
  lpB = pB;
  int *ldummy;
  ldummy = dummy;

  *lpA = 10;
  {
    int *l2pA;
    l2pA = lpA;
    int *l2pB;
    l2pB = lpB;
    int *l2dummy;
    l2dummy = ldummy;
    *l2pA = 42;
    *l2pB = 43;

    *l2dummy = 99;
  }
  return *lpA;
}
// CHECK-LABEL: @test_rr_local_nnn(
// CHECK: ret i32 42

int test_rr_local_nnr(int *pA, int *pB, int *dummy) {
  int *__restrict lpA;
  lpA = pA;
  int *__restrict lpB;
  lpB = pB;
  int *ldummy;
  ldummy = dummy;

  *lpA = 10;
  {
    int *l2pA;
    l2pA = lpA;
    int *l2pB;
    l2pB = lpB;
    int *__restrict l2dummy;
    l2dummy = ldummy;
    *l2pA = 42;
    *l2pB = 43;

    *l2dummy = 99;
  }
  return *lpA;
}
// CHECK-LABEL: @test_rr_local_nnr(
// CHECK: ret i32 42
