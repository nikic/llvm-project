; RUN: not llc -mtriple=msp430 < %s 2>&1 | FileCheck %s

; Targets need to opt into support for explicit extension widths on arguments.

; CHECK: error: argument extension width 16 is not supported by the target
define i16 @arg_i8_zext16(i8 zeroext(16) %x) {
  %ext = zext i8 %x to i16
  ret i16 %ext
}
