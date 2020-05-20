; ModuleID = 't2.ll'
source_filename = "../../../code/t2.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %k.0 = phi i32 [ 0, %entry ], [ %add2, %if.end ]
  %cmp = icmp slt i32 %k.0, 100
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %cmp1 = icmp slt i32 0, %k.0
  br i1 %cmp1, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  %add = add nsw i32 0, 1
  %sub = sub nsw i32 %k.0, 1
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %add2 = add nsw i32 %k.0, 1
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret i32 %k.0
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
