// sfg-check: check resulting chains
// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// check how restrict propagation wrt inlining works

//#define INLINE __attribute__((alwaysinline))
//#define INLINE inline
#define INLINE

// Variations:
// - n : no restrict
// - a : argument restrict
// - R : local restrict

#define ARGRESTRICT_n
#define ARGRESTRICT_a __restrict
#define ARGRESTRICT_R

#define LOCALRESTRICT_n(T, A, B) T A = B
#define LOCALRESTRICT_a(T, A, B) T A = B
#define LOCALRESTRICT_R(T, A, B) T __restrict A = B

#define CREATE_ALL(CS) \
  CS(n, n)             \
  CS(a, n)             \
  CS(n, a)             \
  CS(a, a)             \
  CS(R, n)             \
  CS(n, R)             \
  CS(R, R)

#define CREATE_SET(A, B)                                                    \
  INLINE int set_##A##B(int *ARGRESTRICT_##A pA, int *ARGRESTRICT_##B pB) { \
    LOCALRESTRICT_##A(int *, lpA, pA);                                      \
    LOCALRESTRICT_##B(int *, lpB, pB);                                      \
    *lpA = 42;                                                              \
    *lpB = 99;                                                              \
    return *lpA;                                                            \
  }

#define CREATE_CALL_SET1(A, B)                      \
  int test01_nn_call_set_##A##B(int *pA, int *pB) { \
    set_##A##B(pA, pB);                             \
    return *pA;                                     \
  }

#define CREATE_CALL_SET2(A, B)                                                            \
  int test02_##A##B##_call_set_##A##B(int *ARGRESTRICT_##A pA, int *ARGRESTRICT_##B pB) { \
    LOCALRESTRICT_##A(int *, lpA, pA);                                                    \
    LOCALRESTRICT_##B(int *, lpB, pB);                                                    \
    set_##A##B(lpA, lpB);                                                                 \
    return *lpA;                                                                          \
  }

#define CREATE_CALL_SET3(A, B)                                                        \
  int test03_##A##B##_call_set_nn(int *ARGRESTRICT_##A pA, int *ARGRESTRICT_##B pB) { \
    LOCALRESTRICT_##A(int *, lpA, pA);                                                \
    LOCALRESTRICT_##B(int *, lpB, pB);                                                \
    set_nn(lpA, lpB);                                                                 \
    return *lpA;                                                                      \
  }

CREATE_ALL(CREATE_SET)
CREATE_ALL(CREATE_CALL_SET1)
CREATE_ALL(CREATE_CALL_SET2)
CREATE_ALL(CREATE_CALL_SET3)

// CHECK-LABEL: @set_nn(
// CHECK-NOT: ret i32 42

// CHECK-LABEL: @set_an(
// CHECK: ret i32 42

// CHECK-LABEL: @set_na(
// CHECK: ret i32 42

// CHECK_LABEL: @set_aa(
// CHECK: ret i32 42

// CHECK-LABEL: @set_Rn(
// CHECK: ret i32 42

// CHECK-LABEL: @set_nR(
// CHECK: ret i32 42

// CHECK-LABEL: @set_RR(
// CHECK: ret i32 42

// CHECK-LABEL: @test01_nn_call_set_nn(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_an(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_na(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_aa(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_Rn(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_nR(
// CHECK-NOT: ret i32 42

//@ NOTE: missed store-load propagation
// CHECK-LABEL: @test01_nn_call_set_RR(
// CHECK-NOT: ret i32 42

// CHECK-LABEL: @test02_nn_call_set_nn(
// CHECK-NOT: ret i32 42

// CHECK-LABEL: @test02_an_call_set_an(
// CHECK: ret i32 42

// CHECK-LABEL: @test02_na_call_set_na(
// CHECK: ret i32 42

// CHECK-LABEL: @test02_aa_call_set_aa(
// CHECK: ret i32 42

// CHECK-LABEL: @test02_Rn_call_set_Rn(
// CHECK: ret i32 42

// CHECK-LABEL: @test02_nR_call_set_nR(
// CHECK: ret i32 42

// CHECK-LABEL: @test02_RR_call_set_RR(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_nn_call_set_nn(
// CHECK-NOT: ret i32 42

// CHECK-LABEL: @test03_an_call_set_nn(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_na_call_set_nn(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_aa_call_set_nn(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_Rn_call_set_nn(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_nR_call_set_nn(
// CHECK: ret i32 42

// CHECK-LABEL: @test03_RR_call_set_nn(
// CHECK: ret i32 42
