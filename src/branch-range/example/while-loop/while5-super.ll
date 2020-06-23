; ModuleID = 'while5.ll'
source_filename = "../../../code/while5.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @fun() #0 {
entry:
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %b.0 = phi i32 [ 0, %entry ], [ %b.1, %while.body ]
  %a.0 = phi i32 [ 1, %entry ], [ %add, %while.body ]
  %k.0 = phi i32 [ -100, %entry ], [ %add4, %while.body ]
  %cmp = icmp slt i32 %k.0, 10
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %add = add nsw i32 %a.0, 1
  %cmp1 = icmp sgt i32 %add, 10
  %cmp2 = icmp slt i32 %add, 50
  %or.cond = and i1 %cmp1, %cmp2
  %add3 = add nsw i32 %b.0, 1
  %b.1 = select i1 %or.cond, i32 %add3, i32 %b.0
  %add4 = add nsw i32 %k.0, 1
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret i32 %a.0
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
