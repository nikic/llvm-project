// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm -o - %s | FileCheck %s

// Label differences in C++ constant initializers (APValue::AddrLabelDiff).
// CHECK: @_ZZ10label_diffiE4offs = internal constant [2 x i32] [i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@_Z10label_diffi, 0) to i64), i64 ptrtoint (ptr edgeaddress(@_Z10label_diffi, 0) to i64)) to i32), i32 trunc (i64 sub (i64 ptrtoint (ptr edgeaddress(@_Z10label_diffi, 1) to i64), i64 ptrtoint (ptr edgeaddress(@_Z10label_diffi, 0) to i64)) to i32)]
// Each template instantiation has its own edge addresses.
// CHECK: @_ZZ2tfILi1EEiiE3tbl = linkonce_odr global [2 x ptr] [ptr edgeaddress(@_Z2tfILi1EEii, 0), ptr edgeaddress(@_Z2tfILi1EEii, 1)]
// CHECK: @_ZZ2tfILi2EEiiE3tbl = linkonce_odr global [2 x ptr] [ptr edgeaddress(@_Z2tfILi2EEii, 0), ptr edgeaddress(@_Z2tfILi2EEii, 1)]

// Arithmetic on a label address is not inbounds.
// CHECK-LABEL: define {{.*}}i32 @_Z10label_diffi(
// CHECK:         getelementptr i8, ptr edgeaddress(@_Z10label_diffi, 0), i64 %
int label_diff(int x) {
  static const int offs[] = {(int)((long)&&a - (long)&&a),
                             (int)((long)&&b - (long)&&a)};
  goto *((char *)&&a + offs[x]);
a:
  return 1;
b:
  return 2;
}

template <int N> int tf(int x) {
  static void *tbl[] = {&&a, &&b};
  goto *tbl[x];
a:
  return N;
b:
  return N + 1;
}
int use_tf() { return tf<1>(0) + tf<2>(1); }

// The address of a label in a discarded if constexpr branch: the label is never
// emitted, which used to produce a blockaddress of a block outside the function.
// CHECK-LABEL: define {{.*}}ptr @_Z15discarded_labelv(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    ret ptr edgeaddress(@_Z15discarded_labelv, 0)
// CHECK-NEXT:  }
void *discarded_label() {
  if constexpr (false) {
  l:;
  }
  return &&l;
}
