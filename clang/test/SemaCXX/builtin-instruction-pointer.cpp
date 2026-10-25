// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++20 -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple x86_64-linux-gnu -std=c++20 -fsyntax-only -verify %s -fexperimental-new-constant-interpreter

static_assert(!__has_constexpr_builtin(__builtin_instruction_pointer));

constexpr void *p = __builtin_instruction_pointer(); // expected-error {{constexpr variable 'p' must be initialized by a constant expression}}

constexpr bool f() { // expected-error {{constexpr function never produces a constant expression}}
  return __builtin_instruction_pointer() != nullptr; // expected-note {{subexpression not valid in a constant expression}}
}

constinit void *q = __builtin_instruction_pointer(); // expected-error {{variable does not have a constant initializer}} \
                                                    // expected-note {{required by 'constinit' specifier here}} \
                                                    // expected-note {{subexpression not valid in a constant expression}}

// Dynamic initialization is fine.
void *r = __builtin_instruction_pointer();
void *g() {
  static void *s = __builtin_instruction_pointer();
  return s;
}
