// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// Check that unnecessary llvm.provenance.noalias calls are collapsed

int *test01(int *p, int n) {
  int *__restrict rp;
  rp = p;
  for (int i = 0; i < n; ++i) {
    *rp = 10;
    rp++;
    rp++;
    rp++;
    rp++;
  }
  return rp;
}

// CHECK-LABEL:  @test01(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

int *test02(int *p, int n) {
  int *__restrict rp;
  rp = p;
  rp++;
  rp++;
  rp++;
  rp++;
  for (int i = 0; i < n; ++i) {
    *rp = 10;
    rp++;
    rp++;
    rp++;
    rp++;
  }
  return rp;
}

// CHECK-LABEL: @test02(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

int *test03(int *p, int n) {
  int *__restrict rp;
  rp = p;
  rp++;
  rp++;
  rp++;
  rp++;
  for (int i = 0; i < n; ++i) {
    *rp = 10;
    rp++;
    rp++;
    if (*rp == 42) {
      rp++;
      rp++;
    }
    rp++;
    rp++;
  }
  return rp;
}

// CHECK-LABEL: @test03(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

int *test04(int *p, int n) {
  int *__restrict rp;
  rp = p;
  rp++;
  rp++;
  rp++;
  rp++;
  for (int i = 0; i < n; ++i) {
    *rp = 10;
    rp++;
    rp++;
    switch (*rp) {
    default:
      rp++;
    case 10:
      rp++;
    case 20:
      rp++;
    case 30:
      rp++;
      break;
    }
    if (*rp == 42) {
      rp++;
      rp++;
    }
    rp++;
    rp++;
  }
  return rp;
}

// CHECK-LABEL: @test04(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

int *test05(int *p, int n) {
  int *__restrict rp;
  rp = p;
  rp++;
  rp++;
  rp++;
  rp++;
  for (int i = 0; i < n; ++i) {
    *rp = 10;
    rp++;
    rp++;
    switch (*rp) {
    default:
      rp++;
    case 10:
      rp++;
    case 20:
      rp++;
    case 30:
      rp++;
      break;
    }
    if (*rp == 42) {
      rp++;
      rp++;
    }
    rp++;
    rp++;
  }
  return rp;
}

// CHECK-LABEL: @test05(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

int *test06(int *p, int n) {
  int *__restrict rp1;
  rp1 = p;
  // llvm.provenance.noalias rp1 (p)

  {
    int *__restrict rp;
    rp = p;
    // llvm.provenance.noalias rp (p)
    // llvm.provenance.noalias rp (rp1)
    rp++;
    rp++;
    rp++;
    rp++;
    for (int i = 0; i < n; ++i) {
      *rp = 10;
      rp = rp1;
      rp++;
      rp++;

      switch (*rp) {
      default:
        rp++;
      case 10:
        rp++;
      case 20:
        rp++;
      case 30:
        rp++;
        break;
      }
      if (*rp == 42) {
        rp++;
        rp++;
      }
      rp++;
      rp++;
    }
    return rp;
  }
}

// CHECK-LABEL: @test06(
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK:   = tail call i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
// CHECK-NOT: llvm.provenance.noalias

// CHECK: declare i32* @llvm.provenance.noalias.p0i32.p0i8.p0p0i32.p0p0i32.i64
