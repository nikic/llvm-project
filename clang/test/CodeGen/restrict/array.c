// RUN: %clang_cc1 -triple x86_64-apple-darwin -O1 -disable-llvm-optzns -ffull-restrict %s -emit-llvm -o - | FileCheck %s

int r;
void ex1(int *);

void test_FOO_local(int *pA, int *pB, int *pC) {
  int *restrict tmp[3] = {pA, pB, pC};
  *tmp[0] = 42;
  *tmp[1] = 43;
}

// CHECK-LABEL: void @test_FOO_local(
// CHECK:  [[tmp:%.*]] = alloca [3 x i32*], align 16
// CHECK:  [[TMP1:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0a3p0i32.i64([3 x i32*]* [[tmp]], i64 0, metadata [[TAG_6:!.*]])
// CHECK:  [[arrayidx_begin:%.*]] = getelementptr inbounds [3 x i32*], [3 x i32*]* [[tmp]], i64 0, i64 0
// CHECK:  [[arrayidx:%.*]] = getelementptr inbounds [3 x i32*], [3 x i32*]* [[tmp]], i64 0, i64 0
// CHECK:  [[TMP5:%.*]] = load i32*, i32** [[arrayidx]], align 16, !tbaa [[TAG_2:!.*]], !noalias [[TAG_6]]
// CHECK:  [[TMP6:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP5]], i8* [[TMP1]], i32** [[arrayidx]], i64 0, metadata [[TAG_6]]), !tbaa [[TAG_2]], !noalias [[TAG_6]]
// CHECK:  store i32 42, i32* [[TMP6]], align 4, !tbaa [[TAG_9:!.*]], !noalias [[TAG_6]]
// CHECK:  [[arrayidx2:%.*]] = getelementptr inbounds [3 x i32*], [3 x i32*]* [[tmp]], i64 0, i64 1
// CHECK:  [[TMP7:%.*]] = load i32*, i32** [[arrayidx2]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_6]]
// CHECK:  [[TMP8:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP7]], i8* [[TMP1]], i32** [[arrayidx2]], i64 0, metadata [[TAG_6]]), !tbaa [[TAG_2]], !noalias [[TAG_6]]
// CHECK:  store i32 43, i32* [[TMP8]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_6]]
// CHECK:  ret void

void test_FOO_p(int *restrict p) {
  *p = 42;
}

// define void @test_FOO_p(i32* noalias %p) #0 {
// CHECK-LABEL: void @test_FOO_p(
// CHECK:       [[p_addr:%.*]] = alloca i32*, align 8
// CHECK-NEXT:  [[TMP0:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** [[p_addr]], i64 0, metadata [[TAG_11:!.*]])
// CHECK-NEXT:  store i32* [[p:%.*]], i32** [[p_addr]], align 8, !tbaa [[TAG_2:!.*]], !noalias [[TAG_11]]
// CHECK-NEXT:  [[TMP1:%.*]] = load i32*, i32** [[p_addr]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_11]]
// CHECK-NEXT:  [[TMP2:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP1]], i8* [[TMP0]], i32** [[p_addr]], i64 0, metadata [[TAG_11]]), !tbaa [[TAG_2]], !noalias [[TAG_11]]
// CHECK-NEXT:  store i32 42, i32* [[TMP2]], align 4, !tbaa [[TAG_9:!.*]], !noalias [[TAG_11]]
// CHECK-NEXT:  ret void

void test_FOO_pp(int *restrict *p) {
  *p[0] = 42;
}

// define void @test_FOO_pp(i32** %p) #0 !noalias !14 {
// CHECK: void @test_FOO_pp(i32** [[p:%.*]]) #0 !noalias [[TAG_14:!.*]] {
// CHECK:       [[p_addr:%.*]] = alloca i32**, align 8
// CHECK-NEXT:  store i32** [[p]], i32*** [[p_addr]], align 8, !tbaa [[TAG_2:!.*]], !noalias [[TAG_14:!.*]]
// CHECK-NEXT:  [[TMP0:%.*]] = load i32**, i32*** [[p_addr]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_14]]
// CHECK-NEXT:  [[arrayidx:%.*]] = getelementptr inbounds i32*, i32** [[TMP0]], i64 0
// CHECK-NEXT:  [[TMP1:%.*]] = load i32*, i32** [[arrayidx]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_14]]
// CHECK-NEXT:  [[TMP2:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP1]], i8* null, i32** [[arrayidx]], i64 0, metadata [[TAG_14]]), !tbaa [[TAG_2]], !noalias [[TAG_14]]
// CHECK-NEXT:  store i32 42, i32* [[TMP2]], align 4, !tbaa [[TAG_9:!.*]], !noalias [[TAG_14]]
// CHECK-NEXT:  ret void
