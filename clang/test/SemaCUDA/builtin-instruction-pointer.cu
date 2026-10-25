// The builtin is supported on the host (x86_64), but not on the device
// (nvptx64). Only uses in code that is emitted for the device are diagnosed
// during device compilation.

// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -aux-triple nvptx64-nvidia-cuda \
// RUN:   -fsyntax-only -verify=host %s
// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -aux-triple x86_64-unknown-linux-gnu \
// RUN:   -fcuda-is-device -fsyntax-only -verify=dev %s

// host-no-diagnostics

#include "Inputs/cuda.h"

#if defined(__CUDA_ARCH__) == __has_builtin(__builtin_instruction_pointer)
#error "__has_builtin(__builtin_instruction_pointer) should be true only on the host"
#endif

void *host_fn(void) { return __builtin_instruction_pointer(); }

__device__ void *device_fn(void) {
  return __builtin_instruction_pointer(); // dev-error {{builtin is not supported on this target}}
}

inline __host__ __device__ void *hd_fn(void) {
  return __builtin_instruction_pointer(); // dev-error {{builtin is not supported on this target}}
}

inline __host__ __device__ void *hd_unused(void) {
  return __builtin_instruction_pointer();
}

__global__ void kernel(void **p) {
  *p = hd_fn(); // dev-note {{called by 'kernel'}}
}
