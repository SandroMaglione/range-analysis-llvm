; ModuleID = 'nested.ll'
source_filename = "../../../code/nested.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @fun() #0 {
entry:
  br label %while.cond

while.cond:                                       ; preds = %for.end, %entry
  %a.0 = phi i32 [ 5, %entry ], [ %a.1, %for.end ]
  %k.0 = phi i32 [ 5, %entry ], [ %add2, %for.end ]
  %cmp = icmp slt i32 %k.0, 20
  br i1 %cmp, label %for.cond, label %while.end

for.cond:                                         ; preds = %while.cond, %for.body
  %j.0 = phi i32 [ %inc, %for.body ], [ -10, %while.cond ]
  %a.1 = phi i32 [ %add, %for.body ], [ %a.0, %while.cond ]
  %cmp1 = icmp slt i32 %j.0, 0
  br i1 %cmp1, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %add = add nsw i32 %a.1, 3
  %inc = add nsw i32 %j.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %add2 = add nsw i32 %k.0, 1
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret i32 %a.0
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
