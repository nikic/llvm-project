// RUN: %clang_cc1 -triple x86_64-apple-macosx -fsyntax-only -verify %s

__attribute__((objc_root_class))
@interface I
- (void *)noGoto;
- (void)withGoto:(int)i;
@end

@implementation I
- (void *)noGoto {
  void *p = &&l; // expected-warning {{address of label 'l' is not guaranteed to point to the label when the function contains no indirect goto statement}} \
                 // expected-note {{use '__builtin_instruction_pointer()' to obtain the address of the current code location}}
l:
  return p;
}
- (void)withGoto:(int)i {
  static void *tbl[] = {&&a, &&b};
  goto *tbl[i];
a:
b:;
}
@end
