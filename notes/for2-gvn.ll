; ModuleID = './/for2.ll'
source_filename = "../../../code//for2.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @fun() #0 {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %a.0 = phi i32 [ 0, %entry ], [ %sub, %for.body ]
  %j.0 = phi i32 [ 10, %entry ], [ %dec, %for.body ]
  %cmp = icmp sgt i32 %j.0, 0
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %sub = sub nsw i32 %a.0, 5
  %dec = add nsw i32 %j.0, -1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret i32 %a.0
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
