// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict  -disable-llvm-passes %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict  -disable-llvm-passes %s -emit-llvm -o - | FileCheck %s

//#define __restrict volatile

// this test checks for what variables a restrict scope is created

struct FOO_ipp {
  int **p;
};

struct FOO_irpp {
  int *__restrict *p;
};

struct FOO_iprp {
  int dummy;
  int **__restrict p;
};

struct FOO_irprp {
  int *__restrict *__restrict p;
};

struct FOO_NESTED {
  struct FOO_iprp m;
};

struct FOO_NESTED_A {
  struct FOO_iprp m[2][3][4];
};

typedef struct FOO_NESTED FUM;
typedef int *__restrict t_irp;

int foo(int **p) {
  struct FOO_ipp m1;               // no
  struct FOO_irpp m2;              // no
  struct FOO_iprp m3;              // yes
  struct FOO_irprp m4;             // yes
  struct FOO_NESTED m5;            // yes
  struct FOO_NESTED m6[2][4][5];   // yes
  struct FOO_NESTED_A m7[2][4][5]; // yes
  t_irp p0;                        // yes
  FUM m8[3];                       // yes
  int *a1[2];                      // no
  int **a2[2];                     // no
  int *__restrict *a3[2];          // no
  int **__restrict a4[2];          // yes
  int **__restrict a5[2][3][4];    // yes
  int *__restrict *a6;             // no
  m1.p = p;
  m2.p = p;
  m3.p = p;
  m4.p = p;
  a1[0] = *p;
  a1[1] = *p;

  return **m1.p + **m2.p + **m3.p + **m4.p + *a1[0] + *a1[1];
}

// check the scopes of various variables
// CHECK-LABEL: @foo(

// the local variables:
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK: alloca
// CHECK-NOT: alloca

// the local variables that have a restrict scope:
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK: llvm.noalias.decl
// CHECK-NOT: llvm.noalias.decl
// CHECK: ret i32

// the restrict related metadata
// CHECK: foo: unknown scope
// CHECK-NEXT: foo
// CHECK-NOT:  foo:
// CHECK:      foo: m3
// CHECK-NEXT: foo: m4
// CHECK-NEXT: foo: m5
// CHECK-NEXT: foo: m6
// CHECK-NEXT: foo: m7
// CHECK-NEXT: foo: p0
// CHECK-NEXT: foo: m8
// CHECK-NEXT: foo: a4
// CHECK-NEXT: foo: a5
// CHECK-NOT:  foo:
