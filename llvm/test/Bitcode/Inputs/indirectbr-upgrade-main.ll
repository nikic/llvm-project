target triple = "x86_64-unknown-linux-gnu"

declare i32 @other()
declare ptr @get_tbl()

define i32 @main() {
  %x = call i32 @other()
  %t = call ptr @get_tbl()
  ret i32 %x
}
