target triple = "m68k-unknown-elf"

@counter = global i32 7, align 4

declare void @sink(i32)

define i32 @add(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  ret i32 %sum
}

define void @bump_and_sink(i32 %delta) {
entry:
  %old = load i32, ptr @counter, align 4
  %next = add i32 %old, %delta
  store i32 %next, ptr @counter, align 4
  call void @sink(i32 %next)
  ret void
}
