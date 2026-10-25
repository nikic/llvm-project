; RUN: llc -mtriple=hexagon < %s
; REQUIRES: asserts

; Function Attrs: nounwind
define void @f0() #0 {
b0:
  call void @f1(ptr edgeaddress(@f0, 0))
  br label %b1

b1:                                               ; preds = %b2, %b0
  ret void

b2:                                               ; No predecessors!
  indirectswitch ptr undef, [ ptr edgeaddress(@f0, 0), label %b1 ]
}

declare void @f1(...)

attributes #0 = { nounwind }
