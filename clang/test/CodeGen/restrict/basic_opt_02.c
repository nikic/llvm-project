// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

int foo1(int *a, int *restrict b, int c) {
  *a = 0;
  *b = c;
  return *a; // OK: returns 0
}
// CHECK: @foo1
// CHECK: ret i32 0

int foo2(int *a, int *restrict b, int c) {
  int *bc = b + c;
  *a = 0;
  *bc = c;   // OK: bc keeps the restrictness
  return *a; // returns 0
}
// CHECK: @foo2
// CHECK: ret i32 0

static int *copy(int *b) { return b; }

int foo3(int *a, int *restrict b, int c) {
  int *bc = copy(b); // a fix to support this is in the works
  *a = 0;
  *bc = c;
  return *a;
}
// CHECK: @foo3
// CHECK: ret i32 0

// Finally:
inline void update(int *p, int c) { *p = c; }

int foo6(int *a, int *b, int c) {
  int *restrict bc = b; // local restrict
  *a = 0;
  update(bc, c); // Oops: inlining loses local restrict annotation
  return *a;
}

// CHECK: @foo6
// CHECK: ret i32 0

// Notice the difference with:
int foo7(int *a, int *restrict b, int c) {
  *a = 0;
  update(b, c); // restrict argument preserved after inlining.
  return *a;    // returns 0
}

// CHECK: @foo7
// CHECK: ret i32 0
