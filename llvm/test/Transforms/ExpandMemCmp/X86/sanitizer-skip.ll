; RUN: opt -S -passes=expand-memcmp -mtriple=x86_64-unknown-unknown < %s | FileCheck %s
;
; Verify that ExpandMemCmp does not expand memcmp calls in functions with
; sanitizer attributes, so sanitizer interceptors can catch memory errors.

declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)

; CHECK-LABEL: @cmp_asan(
; CHECK:         call i32 @memcmp
define i32 @cmp_asan(ptr nocapture readonly %x, ptr nocapture readonly %y) sanitize_address {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 4)
  ret i32 %call
}

; CHECK-LABEL: @cmp_msan(
; CHECK:         call i32 @memcmp
define i32 @cmp_msan(ptr nocapture readonly %x, ptr nocapture readonly %y) sanitize_memory {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 4)
  ret i32 %call
}

; CHECK-LABEL: @cmp_tsan(
; CHECK:         call i32 @memcmp
define i32 @cmp_tsan(ptr nocapture readonly %x, ptr nocapture readonly %y) sanitize_thread {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 4)
  ret i32 %call
}

; CHECK-LABEL: @cmp_hwasan(
; CHECK:         call i32 @memcmp
define i32 @cmp_hwasan(ptr nocapture readonly %x, ptr nocapture readonly %y) sanitize_hwaddress {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 4)
  ret i32 %call
}

; CHECK-LABEL: @cmp_no_sanitizer(
; CHECK-NOT:     call i32 @memcmp
define i32 @cmp_no_sanitizer(ptr nocapture readonly %x, ptr nocapture readonly %y) {
  %call = tail call i32 @memcmp(ptr %x, ptr %y, i64 4)
  ret i32 %call
}
