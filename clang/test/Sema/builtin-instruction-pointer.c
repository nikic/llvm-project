// Supported targets.
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple i686-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple aarch64-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple armv7-linux-gnueabihf -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple thumbv7m-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple thumbv6t2-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple armv4t-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple loongarch64-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple riscv64-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple s390x-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple powerpc64le-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple powerpc-aix -fsyntax-only -verify=expected,supported -DSUPPORTED %s

// RUN: %clang_cc1 -triple thumbv6m-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple thumbv8m.base-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple thumbv4t-none-eabi -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple mips-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple mips64-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple mipsisa32r6-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple mipsel-linux-gnu -target-feature +mips16 -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple mipsel-linux-gnu -target-feature +micromips -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple sparc-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple sparcv9-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple hexagon-unknown-elf -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple avr-unknown-unknown -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple lanai-unknown-unknown -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple msp430-unknown-unknown -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple ve-unknown-linux-gnu -fsyntax-only -verify=expected,supported -DSUPPORTED %s
// RUN: %clang_cc1 -triple xcore-unknown-unknown -fsyntax-only -verify=expected,supported -DSUPPORTED %s

// Unsupported targets.
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -fsyntax-only -verify=expected,unsupported %s
// RUN: %clang_cc1 -triple wasm64-unknown-unknown -fsyntax-only -verify=expected,unsupported %s
// RUN: %clang_cc1 -triple bpfel-unknown-none -fsyntax-only -verify=expected,unsupported %s
// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -fsyntax-only -verify=expected,unsupported %s
// RUN: %clang_cc1 -triple amdgcn-amd-amdhsa -fsyntax-only -verify=expected,unsupported %s
// RUN: %clang_cc1 -triple spirv64-unknown-unknown -fsyntax-only -verify=expected,unsupported %s

#if __has_builtin(__builtin_instruction_pointer)
#ifndef SUPPORTED
#error "__has_builtin(__builtin_instruction_pointer) should be false"
#endif
#else
#ifdef SUPPORTED
#error "__has_builtin(__builtin_instruction_pointer) should be true"
#endif
#endif

void *f(void) {
  return __builtin_instruction_pointer(); // unsupported-error {{builtin is not supported on this target}}
}

unsigned long this_ip(void) {
  // The Linux kernel's _THIS_IP_ use case.
  return ({ (unsigned long)__builtin_instruction_pointer(); }); // unsupported-error {{builtin is not supported on this target}}
}

void *too_many_args(void) {
  return __builtin_instruction_pointer(0); // expected-error {{too many arguments to function call, expected 0, have 1}}
}

#ifdef SUPPORTED
// Not a constant expression.
void *global = __builtin_instruction_pointer(); // supported-error {{initializer element is not a compile-time constant}}

void *local_static(void) {
  static void *p = __builtin_instruction_pointer(); // supported-error {{initializer element is not a compile-time constant}}
  return p;
}

_Static_assert(__builtin_instruction_pointer() != 0, ""); // supported-error {{static assertion expression is not an integral constant expression}}
#endif
