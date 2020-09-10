// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -triple i386-unknown-linux-gnu   -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// restrict member pointers and inlining - basic functionality test

struct FOO {
  int *__restrict p;
};

struct FOO_plain {
  int *pA;
  int *pB;
};

int test01_p_pp(int c, int *pA, int *pB) {
  int *__restrict rpA;
  rpA = pA;
  int *__restrict rpB;
  rpB = pB;

  int *p = c ? rpA : rpB;

  return *p;
}
// CHECK-LABEL: @test01_p_pp(
// CHECK:      @llvm.noalias.decl
// CHECK-NEXT: @llvm.noalias.decl
// CHECK-NEXT: icmp
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: select
// CHECK-NEXT: select
// CHECK-NEXT: load
// CHECK-NEXT: ret i32

int test01_p_ss(int c, int *pA, int *pB) {
  struct FOO spA;
  spA.p = pA;
  struct FOO spB;
  spB.p = pB;

  int *p = c ? spA.p : spB.p;

  return *p;
}
// CHECK-LABEL: @test01_p_ss(
// CHECK:      @llvm.noalias.decl
// CHECK-NEXT: @llvm.noalias.decl
// CHECK-NEXT: icmp
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: select
// CHECK-NEXT: select
// CHECK-NEXT: load
// CHECK-NEXT: ret i32

int test01_s_ss(int c, int *pA, int *pB) {
  struct FOO spA;
  spA.p = pA;
  struct FOO spB;
  spB.p = pB;

  {
    struct FOO p = c ? spA : spB;

    return *p.p;
  }
}
// CHECK-LABEL: @test01_s_ss(
// CHECK:      @llvm.noalias.decl
// CHECK-NEXT: @llvm.noalias.decl
// CHECK-NEXT: @llvm.noalias.decl
// CHECK-NEXT: icmp
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: select
// CHECK-NEXT: select
// CHECK-NEXT: @llvm.provenance.noalias
// CHECK-NEXT: load
// CHECK-NEXT: ret i32

// FIXME: this one currently results in bad code :(
int test01_ps_ss(int c, int *pA, int *pB) {
  struct FOO spA;
  spA.p = pA;
  struct FOO spB;
  spB.p = pB;

  struct FOO *p = c ? &spA : &spB;

  return *p->p;
}
// CHECK-LABEL: @test01_ps_ss(
// CHECK: ret i32

int test01_ps_psps(int c, struct FOO *ppA, struct FOO *ppB) {
  struct FOO *p = c ? ppA : ppB;

  return *p->p;
}
// CHECK-LABEL: @test01_ps_psps(
// CHECK: icmp
// CHECK-NEXT: select
// CHECK-NEXT: getelementptr
// CHECK-NEXT: load
// CHECK-NEXT: llvm.provenance.noalias
// CHECK-NEXT: load
// CHECK-NEXT: ret i32
