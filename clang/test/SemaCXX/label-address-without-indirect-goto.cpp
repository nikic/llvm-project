// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -std=c++20 -Wno-return-stack-address -verify=expected,ip %s
// RUN: %clang_cc1 -triple wasm32-unknown-unknown -fsyntax-only -std=c++20 -Wno-return-stack-address -verify=expected %s

#define _THIS_IP_ ({ __label__ __here; __here: (unsigned long)&&__here; })

unsigned long this_ip() {
  return _THIS_IP_; // expected-warning {{address of label '__here' is not guaranteed to point to the label when the function contains no indirect goto statement}} \
                    // ip-note {{use '__builtin_instruction_pointer()' to obtain the address of the current code location}}
}

// Templates are diagnosed once, in the template definition, and not again for
// each instantiation.
template <typename T> void *tmpl() {
l:
  return &&l; // expected-warning {{address of label 'l'}} \
              // ip-note {{__builtin_instruction_pointer()}}
}
template void *tmpl<int>();
template void *tmpl<long>();

template <typename T> void tmpl_goto(T i) {
  static void *tbl[] = {&&a, &&b};
  goto *tbl[i];
a:
b:;
}
template void tmpl_goto<int>(int);

template <typename T> struct ClassTmpl {
  static void *f() {
  l:
    return &&l; // expected-warning {{address of label 'l'}} \
                // ip-note {{__builtin_instruction_pointer()}}
  }
};
void use_class_tmpl() {
  ClassTmpl<int>::f();
  ClassTmpl<char>::f();
}

struct S {
  void *f() {
  l:
    return &&l; // expected-warning {{address of label 'l'}} \
                // ip-note {{__builtin_instruction_pointer()}}
  }
};

// Lambdas are separate functions.
void lambdas() {
  [] {
  l:
    return &&l; // expected-warning {{address of label 'l'}} \
                // ip-note {{__builtin_instruction_pointer()}}
  }();
  [] {
    void *p = &&l;
    goto *p;
  l:;
  }();
  void *p = &&m; // expected-warning {{address of label 'm'}} \
                 // ip-note {{__builtin_instruction_pointer()}}
  (void)p;
  [] {
    void *q = &&n;
    goto *q;
  n:;
  }();
m:;
}

template <typename T> void generic_lambda_in_template() {
  auto f = [](auto x) {
  l:
    return &&l; // expected-warning {{address of label 'l'}} \
                // ip-note {{__builtin_instruction_pointer()}}
  };
  f(1);
  f(2.0);
}
template void generic_lambda_in_template<int>();

// The indirect goto is still present in the definition, so this is not
// diagnosed, even though it is discarded.
void discarded_goto(void *p) {
  p = &&l;
  if constexpr (false)
    goto *p;
l:;
}
