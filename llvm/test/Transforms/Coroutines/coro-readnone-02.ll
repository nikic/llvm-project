; Tests that the readnone function which don't cross suspend points could be optimized expectly after split.
;
; RUN: opt < %s -S -passes='default<O3>' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_SPLITTED
; RUN: opt < %s -S -passes='coro-split,early-cse,simplifycfg' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_SPLITTED
; RUN: opt < %s -S -passes='coro-split,gvn,simplifycfg' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_SPLITTED
; RUN: opt < %s -S -passes='coro-split,newgvn,simplifycfg' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_SPLITTED
; RUN: opt < %s -S -passes='early-cse' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_UNSPLITTED
; RUN: opt < %s -S -passes='gvn' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_UNSPLITTED
; RUN: opt < %s -S -passes='newgvn' -opaque-pointers | FileCheck %s --check-prefixes=CHECK_UNSPLITTED

define ptr @f() "coroutine.presplit" {
entry:
  %id = call token @llvm.coro.id(i32 0, i8* null, i8* null, i8* null)
  %size = call i32 @llvm.coro.size.i32()
  %alloc = call i8* @malloc(i32 %size)
  %hdl = call i8* @llvm.coro.begin(token %id, i8* %alloc)
  %sus_result = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %sus_result, label %suspend [i8 0, label %resume
                                         i8 1, label %cleanup]
resume:
  %i = call i32 @readnone_func() readnone
  ; noop call to break optimization to combine two consecutive readonly calls.
  call void @nop()
  %j = call i32 @readnone_func() readnone
  %cmp = icmp eq i32 %i, %j
  br i1 %cmp, label %same, label %diff

same:
  call void @print_same()
  br label %cleanup

diff:
  call void @print_diff()
  br label %cleanup

cleanup:
  %mem = call i8* @llvm.coro.free(token %id, i8* %hdl)
  call void @free(i8* %mem)
  br label %suspend

suspend:
  call i1 @llvm.coro.end(i8* %hdl, i1 0)
  ret i8* %hdl
}

;
; CHECK_SPLITTED-LABEL: f.resume(
; CHECK_SPLITTED-NEXT:  :
; CHECK_SPLITTED-NEXT:    call i32 @readnone_func() #[[ATTR_NUM:[0-9]+]]
; CHECK_SPLITTED-NEXT:    call void @nop()
; CHECK_SPLITTED-NEXT:    call void @print_same()
;
; CHECK_SPLITTED: attributes #[[ATTR_NUM]] = { readnone }
;
; CHECK_UNSPLITTED-LABEL: @f(
; CHECK_UNSPLITTED: br i1 %cmp, label %same, label %diff
; CHECK_UNSPLITTED-EMPTY:
; CHECK_UNSPLITTED-NEXT: same:
; CHECK_UNSPLITTED-NEXT:   call void @print_same()
; CHECK_UNSPLITTED-NEXT:   br label
; CHECK_UNSPLITTED-EMPTY:
; CHECK_UNSPLITTED-NEXT: diff:
; CHECK_UNSPLITTED-NEXT:   call void @print_diff()
; CHECK_UNSPLITTED-NEXT:   br label

declare i32 @readnone_func() readnone
declare void @nop()

declare void @print_same()
declare void @print_diff()
declare i8* @llvm.coro.free(token, i8*)
declare i32 @llvm.coro.size.i32()
declare i8  @llvm.coro.suspend(token, i1)

declare token @llvm.coro.id(i32, i8*, i8*, i8*)
declare i1 @llvm.coro.alloc(token)
declare i8* @llvm.coro.begin(token, i8*)
declare i1 @llvm.coro.end(i8*, i1)

declare noalias i8* @malloc(i32)
declare void @free(i8*)
