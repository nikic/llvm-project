// RUN: %clang_cc1 -triple x86_64-apple-darwin -O1 -disable-llvm-optzns -ffull-restrict %s -emit-llvm -o - | FileCheck %s

int r;
void ex1(int *);

struct FOO {
  int *restrict rp0;
  int *restrict rp1;
  int *restrict rp2;
};

void test_FOO_local(int *pA, int *pB, int *pC) {
  struct FOO tmp = {pA, pB, pC};
  *tmp.rp0 = 42;
  *tmp.rp1 = 43;
}
// CHECK-LABEL: void @test_FOO_local(
// CHECK:  [[tmp:%.*]] = alloca %struct.FOO, align 8
// CHECK:  [[TMP1:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FOOs.i64(%struct.FOO* [[tmp]], i64 0, metadata [[TAG_6:!.*]])
// CHECK:  [[rp0:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[tmp]], i32 0, i32 0
// CHECK:  [[rp01:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[tmp]], i32 0, i32 0
// CHECK:  [[TMP5:%.*]] = load i32*, i32** [[rp01]], align 8, !tbaa [[TAG_9:!.*]], !noalias [[TAG_6]]
// CHECK:  [[TMP6:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP5]], i8* [[TMP1]], i32** [[rp01]], i64 0, metadata [[TAG_6]]), !tbaa [[TAG_9]], !noalias [[TAG_6]]
// CHECK:  store i32 42, i32* [[TMP6]], align 4, !tbaa [[TAG_13:!.*]], !noalias [[TAG_6]]
// CHECK:  [[rp12:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[tmp]], i32 0, i32 1
// CHECK:  [[TMP7:%.*]] = load i32*, i32** [[rp12]], align 8, !tbaa [[TAG_11:!.*]], !noalias [[TAG_6]]
// CHECK:  [[TMP8:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP7]], i8* [[TMP1]], i32** [[rp12]], i64 0, metadata [[TAG_6]]), !tbaa [[TAG_11]], !noalias [[TAG_6]]
// CHECK:  store i32 43, i32* [[TMP8]], align 4, !tbaa [[TAG_13]], !noalias [[TAG_6]]
// CHECK:  ret void

void test_FOO_arg_pointer(struct FOO *p) {
  *p->rp0 = 42;
  *p->rp1 = 43;
}

// define void @test_FOO_arg_pointer(%struct.FOO* %p) #0 !noalias !15 {
// CHECK: void @test_FOO_arg_pointer(%struct.FOO* [[p:%.*]]) #0 !noalias [[TAG_15:!.*]] {
// CHECK:       [[p_addr:%.*]] = alloca %struct.FOO*, align 8
// CHECK-NEXT:  store %struct.FOO* [[p]], %struct.FOO** [[p_addr]], align 8, !tbaa [[TAG_2:!.*]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[TMP0:%.*]] = load %struct.FOO*, %struct.FOO** [[p_addr]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[rp0:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[TMP0]], i32 0, i32 0
// CHECK-NEXT:  [[TMP1:%.*]] = load i32*, i32** [[rp0]], align 8, !tbaa [[TAG_9:!.*]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[TMP2:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP1]], i8* null, i32** [[rp0]], i64 0, metadata [[TAG_15]]), !tbaa [[TAG_9]], !noalias [[TAG_15]]
// CHECK-NEXT:  store i32 42, i32* [[TMP2]], align 4, !tbaa [[TAG_13:!.*]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[TMP3:%.*]] = load %struct.FOO*, %struct.FOO** [[p_addr]], align 8, !tbaa [[TAG_2]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[rp1:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[TMP3]], i32 0, i32 1
// CHECK-NEXT:  [[TMP4:%.*]] = load i32*, i32** [[rp1]], align 8, !tbaa [[TAG_11:!.*]], !noalias [[TAG_15]]
// CHECK-NEXT:  [[TMP5:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP4]], i8* null, i32** [[rp1]], i64 0, metadata [[TAG_15]]), !tbaa [[TAG_11]], !noalias [[TAG_15]]
// CHECK-NEXT:  store i32 43, i32* [[TMP5]], align 4, !tbaa [[TAG_13]], !noalias [[TAG_15]]
// CHECK-NEXT:  ret void

void test_FOO_arg_value(struct FOO p) {
  *p.rp0 = 42;
  *p.rp1 = 43;
}
// NOTE: the struct is mapped 'byval', the scope will be introduced after inlining.

// define void @test_FOO_arg_value(%struct.FOO* byval(%struct.FOO) align 8 %p) #0 !noalias !18 {
// CHECK: void @test_FOO_arg_value(%struct.FOO* byval(%struct.FOO) align 8 %p) #0 !noalias [[TAG_18:!.*]] {
// CHECK:       [[rp0:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[p:%.*]], i32 0, i32 0
// CHECK-NEXT:  [[TMP0:%.*]] = load i32*, i32** [[rp0]], align 8, !tbaa [[TAG_9:!.*]], !noalias [[TAG_18]]
// CHECK-NEXT:  [[TMP1:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP0]], i8* null, i32** [[rp0]], i64 0, metadata [[TAG_18]]), !tbaa [[TAG_9]], !noalias [[TAG_18]]
// CHECK-NEXT:  store i32 42, i32* [[TMP1]], align 4, !tbaa [[TAG_13:!.*]], !noalias [[TAG_18]]
// CHECK-NEXT:  [[rp1:%.*]] = getelementptr inbounds %struct.FOO, %struct.FOO* [[p]], i32 0, i32 1
// CHECK-NEXT:  [[TMP2:%.*]] = load i32*, i32** [[rp1]], align 8, !tbaa [[TAG_11:!.*]], !noalias [[TAG_18]]
// CHECK-NEXT:  [[TMP3:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP2]], i8* null, i32** [[rp1]], i64 0, metadata [[TAG_18]]), !tbaa [[TAG_11]], !noalias [[TAG_18]]
// CHECK-NEXT:  store i32 43, i32* [[TMP3]], align 4, !tbaa [[TAG_13]], !noalias [[TAG_18]]
// CHECK-NEXT:  ret void

struct FOO test_FOO_pass(struct FOO p) {
  return p;
}

// define void @test_FOO_pass(%struct.FOO* noalias sret align 8 %agg.result, %struct.FOO* byval(%struct.FOO) align 8 %p) #0 !noalias !21 {
// CHECK: void @test_FOO_pass(%struct.FOO* noalias sret align 8 %agg.result, %struct.FOO* byval(%struct.FOO) align 8 %p) #0 !noalias [[TAG_21:!.*]] {
// CHECK:       [[TMP0:%.*]] = call %struct.FOO* @llvm.noalias.copy.guard.p0s_struct.FOOs.p0i8(%struct.FOO* [[p:%.*]], i8* null, metadata [[TAG_24:!.*]], metadata [[TAG_21]])
// CHECK-NEXT:  [[TMP1:%.*]] = bitcast %struct.FOO* [[agg_result:%.*]] to i8*
// CHECK-NEXT:  [[TMP2:%.*]] = bitcast %struct.FOO* [[TMP0]] to i8*
// CHECK-NEXT:  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 [[TMP1]], i8* align 8 [[TMP2]], i64 24, i1 false), !tbaa.struct [[TAG_28:!.*]], !noalias [[TAG_21]]
// CHECK-NEXT:  ret void

struct FUM {
  struct FOO m;
};

void test_FUM_local(int *pA, int *pB, int *pC) {
  struct FUM tmp = {{pA, pB, pC}};
  *tmp.m.rp0 = 42;
  *tmp.m.rp1 = 43;
}

// CHECK-LABEL: void @test_FUM_local(
// CHECK:  [[TMP1:%.*]] = call i8* @llvm.noalias.decl.p0i8.p0s_struct.FUMs.i64(%struct.FUM* [[tmp]], i64 0, metadata [[TAG_29:!.*]])
// CHECK:  [[TMP6:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP5:%.*]], i8* [[TMP1]], i32** [[rp02:%.*]], i64 0, metadata [[TAG_29]]), !tbaa [[TAG_32:!.*]], !noalias [[TAG_29]]
// CHECK:  [[TMP8:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP7:%.*]], i8* [[TMP1]], i32** [[rp14:%.*]], i64 0, metadata [[TAG_29]]), !tbaa [[TAG_34:!.*]], !noalias [[TAG_29]]

void test_FUM_arg_pointer(struct FUM *p) {
  *p->m.rp0 = 42;
  *p->m.rp1 = 43;
}
// define void @test_FUM_arg_pointer(%struct.FUM* %p) #0 !noalias !35 {
// CHECK: void @test_FUM_arg_pointer(%struct.FUM* [[p:%.*]]) #0 !noalias [[TAG_35:!.*]] {
// CHECK:  [[TMP2:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP1:%.*]], i8* null, i32** [[rp0:%.*]], i64 0, metadata [[TAG_35]]), !tbaa [[TAG_32:!.*]], !noalias [[TAG_35]]
// CHECK:  [[TMP5:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP4:%.*]], i8* null, i32** [[rp1:%.*]], i64 0, metadata [[TAG_35]]), !tbaa [[TAG_34:!.*]], !noalias [[TAG_35]]

void test_FUM_arg_value(struct FUM p) {
  *p.m.rp0 = 42;
  *p.m.rp1 = 43;
}

// define void @test_FUM_arg_value(%struct.FUM* byval(%struct.FUM) align 8 %p) #0 !noalias !38 {
// CHECK: void @test_FUM_arg_value(%struct.FUM* byval(%struct.FUM) align 8 %p) #0 !noalias [[TAG_38:!.*]] {
// CHECK:  [[TMP1:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP0:%.*]], i8* null, i32** [[rp0:%.*]], i64 0, metadata [[TAG_38]]), !tbaa [[TAG_32:!.*]], !noalias [[TAG_38]]
// CHECK:  [[TMP3:%.*]] = call i32* @llvm.noalias.p0i32.p0i8.p0p0i32.i64(i32* [[TMP2:%.*]], i8* null, i32** [[rp1:%.*]], i64 0, metadata [[TAG_38]]), !tbaa [[TAG_34:!.*]], !noalias [[TAG_38]]

struct FUM test_FUM_pass(struct FUM p) {
  return p;
}

// define void @test_FUM_pass(%struct.FUM* noalias sret align 8 %agg.result, %struct.FUM* byval(%struct.FUM) align 8 %p) #0 !noalias !41 {
// CHECK: void @test_FUM_pass(%struct.FUM* noalias sret align 8 %agg.result, %struct.FUM* byval(%struct.FUM) align 8 %p) #0 !noalias [[TAG_41:!.*]] {
// CHECK:       [[TMP0:%.*]] = call %struct.FUM* @llvm.noalias.copy.guard.p0s_struct.FUMs.p0i8(%struct.FUM* [[p:%.*]], i8* null, metadata [[TAG_44:!.*]], metadata [[TAG_41]])
// CHECK-NEXT:  [[TMP1:%.*]] = bitcast %struct.FUM* [[agg_result:%.*]] to i8*
// CHECK-NEXT:  [[TMP2:%.*]] = bitcast %struct.FUM* [[TMP0]] to i8*
// CHECK-NEXT:  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 [[TMP1]], i8* align 8 [[TMP2]], i64 24, i1 false), !tbaa.struct [[TAG_28:!.*]], !noalias [[TAG_41]]
// CHECK-NEXT:  ret void

// indices for llvm.noalias.copy.guard

// CHECK: [[TAG_24]] = !{[[TAG_25:!.*]], [[TAG_26:!.*]], [[TAG_27:!.*]]}
// CHECK: [[TAG_25]] = !{i32 -1, i32 0}
// CHECK: [[TAG_26]] = !{i32 -1, i32 1}
// CHECK: [[TAG_27]] = !{i32 -1, i32 2}

// CHECK: [[TAG_44]] = !{[[TAG_45:!.*]], [[TAG_46:!.*]], [[TAG_47:!.*]]}
// CHECK: [[TAG_45]] = !{i32 -1, i32 0, i32 0}
// CHECK: [[TAG_46]] = !{i32 -1, i32 0, i32 1}
// CHECK: [[TAG_47]] = !{i32 -1, i32 0, i32 2}
