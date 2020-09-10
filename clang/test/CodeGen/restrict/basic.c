// RUN: %clang_cc1 -triple x86_64-apple-darwin -O1 -disable-llvm-optzns -ffull-restrict %s -emit-llvm -o - | FileCheck %s

int r;
void ex1(int *);

int *a;
int *foo() {
  int *restrict x = a;
  return x;

  // CHECK-LABEL: i32* @foo(
  // CHECK:  [[x:%.*]] = alloca i32*, align 8
  // CHECK:  [[TMP1:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** [[x]], i64 0, metadata  [[TAG_2:!.*]])
  // CHECK:  [[TMP3:%.*]] = load i32*, i32** [[x]], align 8, !tbaa !5, !noalias [[TAG_2]]
  // CHECK:  [[TMP4:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP3]], i8* [[TMP1]], i32** [[x]], i64 0, metadata !2), !tbaa !5, !noalias [[TAG_2]]
  // CHECK:  ret i32* [[TMP4]]
}

int *a2;
int *foo1(int b) {
  int *restrict x;

  // CHECK-LABEL: define i32* @foo1(i32 %b)
  // CHECK:  [[b_addr:%.*]] = alloca i32, align 4
  // CHECK:  [[x:%.*]] = alloca i32*, align 8
  // CHECK:  [[x2:%.*]] = alloca i32*, align 8
  // CHECK:  [[TMP1:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** [[x]], i64 0, metadata [[TAG_x:!.*]])

  if (b) {
    x = a;
    r += *x;
    ex1(x);

    // CHECK:  [[TMP3:%.*]] = load i32*, i32** @a, align 8, !tbaa [[TAG_5:!.*]], !noalias [[TAG_x_x2:!.*]]
    // CHECK:  store i32* [[TMP3]], i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP4:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP5:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP4]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP6:%.*]] = load i32, i32* [[TMP5]], align 4, !tbaa [[TAG_9:!.*]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP7:%.*]] = load i32, i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]
    // CHECK:  [[add:%.*]] = add nsw i32 [[TMP7]], [[TMP6]]
    // CHECK:  store i32 [[add]], i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP8:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP9:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP8]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  call void @ex1(i32* [[TMP9]]), !noalias [[TAG_x_x2]]

    ++x;
    *x = r;
    ex1(x);

    // CHECK:  [[TMP10:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP11:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP10]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[incdec_ptr:%.*]] = getelementptr inbounds i32, i32* [[TMP11]], i32 1
    // CHECK:  store i32* [[incdec_ptr]], i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP12:%.*]] = load i32, i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP13:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP14:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP13]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  store i32 [[TMP12]], i32* [[TMP14]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP15:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP16:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP15]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  call void @ex1(i32* [[TMP16]]), !noalias [[TAG_x_x2]]

    x += b;
    *x = r;
    ex1(x);

    // CHECK:  [[TMP17:%.*]] = load i32, i32* [[b_addr]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP18:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP19:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP18]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[idx_ext:%.*]] = sext i32 [[TMP17]] to i64
    // CHECK:  [[add_ptr:%.*]] = getelementptr inbounds i32, i32* [[TMP19]], i64 [[idx_ext]]
    // CHECK:  store i32* [[add_ptr]], i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP20:%.*]] = load i32, i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP21:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP22:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP21]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  store i32 [[TMP20]], i32* [[TMP22]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP23:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP24:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP23]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  call void @ex1(i32* [[TMP24]]), !noalias [[TAG_x_x2]]

    int *restrict x2 = a2;
    *x2 = r;
    ex1(x2);

    // CHECK:  [[TMP26:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0p0i32.i64(i32** [[x2]], i64 0, metadata [[TAG_x2:!.*]])
    // CHECK:  [[TMP27:%.*]] = load i32*, i32** @a2, align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  store i32* [[TMP27]], i32** [[x2]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP28:%.*]] = load i32, i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP29:%.*]] = load i32*, i32** [[x2]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP30:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP29]], i8* [[TMP26]], i32** [[x2]], i64 0, metadata [[TAG_x2]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  store i32 [[TMP28]], i32* [[TMP30]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_x_x2]]

    // CHECK:  [[TMP31:%.*]] = load i32*, i32** [[x2]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  [[TMP32:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP31]], i8* [[TMP26]], i32** [[x2]], i64 0, metadata [[TAG_x2]]), !tbaa [[TAG_5]], !noalias [[TAG_x_x2]]
    // CHECK:  call void @ex1(i32* [[TMP32]]), !noalias [[TAG_x_x2]]
  } else {
    x = a2;
    r += *x;
    // CHECK:  [[TMP34:%.*]] = load i32*, i32** @a2, align 8, !tbaa [[TAG_5]], !noalias [[TAG_x]]
    // CHECK:  store i32* [[TMP34]], i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x]]

    // CHECK:  [[TMP35:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x]]
    // CHECK:  [[TMP36:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP35]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x]]
    // CHECK:  [[TMP37:%.*]] = load i32, i32* [[TMP36]], align 4, !tbaa [[TAG_9]], !noalias [[TAG_x]]
    // CHECK:  [[TMP38:%.*]] = load i32, i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x]]
    // CHECK:  [[add1:%.*]] = add nsw i32 [[TMP38]], [[TMP37]]
    // CHECK:  store i32 [[add1]], i32* @r, align 4, !tbaa [[TAG_9]], !noalias [[TAG_x]]
  }

  return x;
  // CHECK:  [[TMP39:%.*]] = load i32*, i32** [[x]], align 8, !tbaa [[TAG_5]], !noalias [[TAG_x]]
  // CHECK:  [[TMP40:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP39]], i8* [[TMP1]], i32** [[x]], i64 0, metadata [[TAG_x]]), !tbaa [[TAG_5]], !noalias [[TAG_x]]
  // CHECK:  ret i32* [[TMP40]]
}

int *bar() {
  int *x = a;
  return x;

  // CHECK-LABEL: define i32* @bar()
  // CHECK-NOT: noalias
  // CHECK: ret i32*
}
