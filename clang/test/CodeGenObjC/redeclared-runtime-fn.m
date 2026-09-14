// RUN: %clang_cc1 -fobjc-exceptions -emit-llvm -o - %s 2>/dev/null | FileCheck %s

// The user is allowed to redeclare a runtime function with a different
// signature. CodeGen must not then apply the attributes it computed for the
// real signature to the user's declaration -- that produces invalid IR.

int objc_sync_enter(void);

@interface A @end

void use_redeclaration(void) { objc_sync_enter(); }

void sync(A *a) { @synchronized (a) { } }

// CHECK: declare i32 @objc_sync_enter()
// CHECK-NOT: declare {{.*}} @objc_sync_enter
