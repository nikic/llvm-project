// RUN: %clang_cc1 %s -verify -fsyntax-only -std=c11 -Wno-label-address-without-indirect-goto

int x(void) { e: b: ; return &&e - &&b < x; } // expected-warning {{ordered comparison between pointer and integer}}
