// RUN: %clang_cc1 -triple x86_64-windows-msvc -emit-llvm %s -o - | FileCheck %s

// CHECK: @constinit = private constant [3 x ptr] [ptr edgeaddress(@main, 0), ptr null, ptr null]

void receivePtrs(void **);

int main(void) {
L:
  receivePtrs((void *[]){ &&L, 0, 0 });
}
