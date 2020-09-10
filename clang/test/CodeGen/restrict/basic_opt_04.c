// RUN: %clang_cc1 -triple x86_64-apple-darwin -O2 -ffull-restrict %s -emit-llvm -o - | FileCheck %s

// verify effect of restrict on optimizations
void dummy_restrict01_n(int *p) {
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict01_a(int *__restrict p) {
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict01_r(int *p_) {
  int *__restrict p = p_;
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict01_R(int *p_) {
  int *__restrict p;
  p = p_;
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict02_n(int *p) {
  p++;
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict02_a(int *__restrict p) {
  p++;
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict02_r(int *p_) {
  int *__restrict p = p_;
  p++;
  if (0) {
    *p = 0xdeadbeef;
  }
}

void dummy_restrict02_R(int *p_) {
  int *__restrict p;
  p = p_;
  p++;
  if (0) {
    *p = 0xdeadbeef;
  }
}

// ---------------------------------

int test01_n(int *pA, int c) {
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}

// CHECK: @test01_n
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_a(int *__restrict pA, int c) {
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}

// CHECK: @test01_a
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_r(int *pA_, int c) {
  int *__restrict pA = pA_;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test01_r
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_R(int *pA_, int c) {
  int *__restrict pA;
  pA = pA_;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test01_R
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_nn(int *pA, int c) {
  dummy_restrict01_n(pA);
  return c;
}
// CHECK: @test01_nn
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_na(int *pA, int c) {
  dummy_restrict01_a(pA);
  return c;
}
// CHECK: @test01_na
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_nr(int *pA, int c) {
  dummy_restrict01_r(pA);
  return c;
}
// CHECK: @test01_nr
// CHECK-NOT: .noalias
// CHECK: ret i32

int test01_nR(int *pA, int c) {
  dummy_restrict01_R(pA);
  return c;
}
// CHECK: @test01_nR
// CHECK-NOT: .noalias
// CHECK: ret i32

// ----------------------------------
int test02_n(int *pA, int c) {
  pA++;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test02_n
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_a(int *__restrict pA, int c) {
  pA++;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test02_a
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_r(int *pA_, int c) {
  int *__restrict pA = pA_;
  pA++;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test02_r
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_R(int *pA_, int c) {
  int *__restrict pA;
  pA = pA_;
  pA++;
  if (0) {
    *pA = 0xdeadbeef;
  }
  return c;
}
// CHECK: @test02_R
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_nn(int *pA, int c) {
  dummy_restrict02_n(pA);
  return c;
}
// CHECK: @test02_nn
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_na(int *pA, int c) {
  dummy_restrict02_a(pA);
  return c;
}
// CHECK: @test02_na
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_nr(int *pA, int c) {
  dummy_restrict02_r(pA);
  return c;
}
// CHECK: @test02_nr
// CHECK-NOT: .noalias
// CHECK: ret i32

int test02_nR(int *pA, int c) {
  dummy_restrict02_R(pA);
  return c;
}
// CHECK: @test02_nR
// CHECK-NOT: .noalias
// CHECK: ret i32

int test11_n(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *p = pA;
    total = total + 1;
  }
  return total;
}
// CHECK: @test11_n
// CHECK-NOT: .noalias
// CHECK: ret i32

int test11_lr(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *__restrict p = pA;
    total = total + 1;
  }
  return total;
}
// CHECK: @test11_lr
// CHECK-NOT: .noalias
// CHECK: ret i32

int test11_lR(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *__restrict p;
    p = pA;
    total = total + 1;
  }
  return total;
}
// CHECK: @test11_lR
// CHECK-NOT: .noalias
// CHECK: ret i32

int test12_n(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *p = pA;
    p++;
    total = total + 1;
  }
  return total;
}
// CHECK: @test12_n
// CHECK-NOT: .noalias
// CHECK: ret i32

int test12_lr(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *__restrict p = pA;
    p++;
    total = total + 1;
  }
  return total;
}
// CHECK: @test12_lr
// CHECK-NOT: .noalias
// CHECK: ret i32

int test12_lR(int *pA) {
  unsigned total = 0;
  for (int i = 0; i < 10; ++i) {
    int *__restrict p;
    p = pA;
    p++;
    total = total + 1;
  }
  return total;
}
// CHECK: @test12_lR
// CHECK-NOT: .noalias
// CHECK: ret i32
