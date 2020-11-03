// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict -fno-noalias-arguments %s -emit-llvm -o - | FileCheck %s

// NOTE: use -no-noalias-arguments to block mapping restrict arguments on the 'noalias
//  attribute which is too strong for restrict

// A number of testcases from our wiki (2018/6/7_llvm_restrict_examples
// As llvm/clang treat __restrict differently in following cases:
int test_arg_restrict_vs_local_restrict_01(int *__restrict pA, int *pB, int *pC) {
  int *tmp = pA;
  *tmp = 42;
  pA = pB;
  *pA = 43;
  *pC = 99;
  return *tmp; // fail: needs a load !!! (either 42 or 43)
}

// CHECK: @test_arg_restrict_vs_local_restrict_01
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99

int test_arg_restrict_vs_local_restrict_02(int *pA_, int *pB, int *pC) {
  int *__restrict pA;
  pA = pA_;
  int *tmp = pA;
  *tmp = 42;
  pA = pB;
  *pA = 43;
  *pC = 99;
  return *tmp; // needs a load !!! (either 42 or 43)
}

// CHECK: @test_arg_restrict_vs_local_restrict_02
// CHECK-NOT: ret i32 42
// CHECK-NOT: ret i32 43
// CHECK-NOT: ret i32 99
