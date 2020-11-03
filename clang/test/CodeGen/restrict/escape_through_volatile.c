// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// a <tt>volatile</tt> pointer can confuse llvm: (p2 depends on p0)
int test_escape_through_volatile_01(int *a_p0) {
  int *__restrict p0;
  p0 = a_p0;
  int *volatile p1 = p0;
  int *p2 = p1;
  *p0 = 42;
  *p2 = 99;

  return *p0; // 42 or 99
}

// CHECK: @test_escape_through_volatile_01
// either a reload or 99, but must never be 42
// CHECK-NOT: ret i32 42

// but not in:
int test_escape_through_volatile_02(int *__restrict p0) {
  int *volatile p1 = p0;
  int *p2 = p1;
  *p0 = 42;
  *p2 = 99;

  return *p0; // 42 or 99
}

// CHECK: @test_escape_through_volatile_02
// either a reload or 99, but must never be 42
// CHECK-NOT: ret i32 42
