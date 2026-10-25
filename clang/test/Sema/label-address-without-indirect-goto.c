// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -fblocks -verify=expected,ip %s
// RUN: %clang_cc1 -triple aarch64-linux-gnu -fsyntax-only -fblocks -verify=expected,ip %s
// RUN: %clang_cc1 -triple mips-linux-gnu -fsyntax-only -fblocks -verify=expected,ip %s
// RUN: %clang_cc1 -triple thumbv6m-none-eabi -fsyntax-only -fblocks -verify=expected,ip %s
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -fsyntax-only -fblocks -verify=expected %s
// RUN: %clang_cc1 -triple bpfel-unknown-none -fsyntax-only -fblocks -verify=expected %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -fblocks -fopenmp -DOPENMP -verify=expected,ip %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -fblocks -Wno-label-address-without-indirect-goto -verify=none %s

// The Linux kernel's _THIS_IP_ pattern.
#define _THIS_IP_ ({ __label__ __here; __here: (unsigned long)&&__here; })

unsigned long this_ip(void) {
  return _THIS_IP_; // expected-warning {{address of label '__here' is not guaranteed to point to the label when the function contains no indirect goto statement}} \
                    // ip-note {{use '__builtin_instruction_pointer()' to obtain the address of the current code location}}
}

// Each expansion declares a separate local label, so each one is diagnosed.
void this_ip_twice(unsigned long *a, unsigned long *b) {
  *a = _THIS_IP_; // expected-warning {{address of label '__here'}} \
                  // ip-note {{__builtin_instruction_pointer()}}
  *b = _THIS_IP_; // expected-warning {{address of label '__here'}} \
                  // ip-note {{__builtin_instruction_pointer()}}
}

// Only the first use of each label is diagnosed.
void *no_goto_table(int i) {
  static void *tbl[] = {&&a, &&b, &&a}; // expected-warning {{address of label 'a'}} \
                                        // expected-warning {{address of label 'b'}} \
                                        // ip-note 2 {{__builtin_instruction_pointer()}}
a:
b:
  return tbl[i];
}

// Ordinary uses of label addresses together with an indirect goto.
int dispatch_table(int i) {
  static void *tbl[] = {&&a, &&b};
  goto *tbl[i];
a:
  return 1;
b:
  return 2;
}

int dispatch_local(int i) {
  void *p = i ? &&a : &&b;
  goto *p;
a:
  return 1;
b:
  return 2;
}

// Label address differences, as used by interpreters.
int dispatch_diff(int i) {
  static const int offsets[] = {&&a - &&a, &&b - &&a};
  goto *(&&a + offsets[i]);
a:
  return 1;
b:
  return 2;
}

// The goto may appear before the label address is taken, or inside a
// statement expression; it all belongs to the same function.
void goto_before(void *p) {
  goto *p;
  p = &&l;
l:;
}

void goto_in_stmt_expr(void) {
  void *p = &&l;
  ({ goto *p; });
l:;
}

void label_in_stmt_expr(void *p) {
  ({ __label__ l; l: p = &&l; goto *p; });
}

// asm goto does not use label addresses and does not count as indirect goto.
void asm_goto_only(void) {
  asm goto("" :::: l);
l:;
}

void asm_goto(void) {
  void *p = &&l; // expected-warning {{address of label 'l'}} \
                 // ip-note {{__builtin_instruction_pointer()}}
  (void)p;
  asm goto("" :::: l);
l:;
}

void asm_goto_and_indirect_goto(void *p) {
  p = &&l;
  asm goto("" :::: l);
  goto *p;
l:;
}

// Blocks are separate functions.
void *sink;
void blocks(void) {
  void (^b1)(void) = ^{
  l:
    sink = &&l; // expected-warning {{address of label 'l'}} \
                        // ip-note {{__builtin_instruction_pointer()}}
  };
  void (^b2)(void) = ^{
    void *p = &&l;
    goto *p;
  l:;
  };
  // The indirect goto in the block does not apply to the enclosing function.
  void *p = &&l; // expected-warning {{address of label 'l'}} \
                 // ip-note {{__builtin_instruction_pointer()}}
  (void)p;
l:;
}

// Nested functions with an indirect goto in the enclosing function.
void block_with_outer_goto(void *p) {
  void (^b)(void) = ^{
  l:;
    void *q = &&l; // expected-warning {{address of label 'l'}} \
                   // ip-note {{__builtin_instruction_pointer()}}
    (void)q;
  };
  p = &&m;
  goto *p;
m:;
}

// Don't warn if the function contains errors, since an indirect goto may have
// been dropped.
struct S { int x; };
void error_recovery(struct S s) {
  void *p = &&l;
  (void)p;
  goto *s; // expected-error {{passing 'struct S' to parameter of incompatible type 'const void *'}} \
           // none-error {{passing 'struct S'}}
l:;
}

#ifdef OPENMP
// Captured regions are emitted as separate functions.
void openmp(void) {
#pragma omp parallel
  {
    void *p = &&l; // expected-warning {{address of label 'l'}} \
                   // ip-note {{__builtin_instruction_pointer()}}
    (void)p;
  l:;
  }
#pragma omp parallel
  {
    void *p = &&m;
    goto *p;
  m:;
  }
}
#endif
