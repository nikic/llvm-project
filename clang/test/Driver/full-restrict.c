// RUN: %clang -### -ffull-restrict -S %s 2>&1 | FileCheck %s --check-prefix=CHECK_FULL
// RUN: %clang -### -fno-full-restrict -S %s 2>&1 | FileCheck %s --check-prefix=CHECK_NO_FULL
// RUN: %clang -### -S %s 2>&1 | FileCheck %s --check-prefix=CHECK_NOTHING

// CHECK_FULL-NOT: -fno-full-restrict
// CHECK_FULL:      -ffull-restrict
// CHECK_FULL-NOT: -fno-full-restrict

// CHECK_NO_FULL-NOT: -ffull-restrict
// CHECK_NO_FULL:     -fno-full-restrict
// CHECK_NO_FULL-NOT: -ffull-restrict

// CHECK_NOTHING-NOT: -fno-full-restrict
// CHECK_NOTHING-NOT: -ffull-restrict
