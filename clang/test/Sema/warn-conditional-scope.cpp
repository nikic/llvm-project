// RUN: %clang_cc1 -fsyntax-only -verify -Wconditional-scope %s

int *get_something();
int *get_something_else();
int *get_something_else_again();

int test() {
  if (int *ptr = get_something()) {
    return ptr[0] * ptr[0];
  }
  else if (int *ptr2 = get_something_else()) {
    // expected-warning@+1{{variable ptr used in else/else if block is out of scope}}
    return ptr[0] * ptr2[0];
  }
  else if (int* ptr3 = get_something_else_again()) {
    // expected-warning@+2{{variable ptr used in else/else if block is out of scope}}
    // expected-warning@+1{{variable ptr2 used in else/else if block is out of scope}}
    return ptr[0] * ptr2[0] * ptr3[0];	
  }
  else {
    return -1;
  }
}
