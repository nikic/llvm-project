// Unsupported targets are rejected by Sema rather than reaching the backend.
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -emit-llvm-only -verify %s
// RUN: %clang_cc1 -triple bpfel-unknown-none -emit-llvm-only -verify %s

void *f(void) {
  return __builtin_instruction_pointer(); // expected-error {{builtin is not supported on this target}}
}
