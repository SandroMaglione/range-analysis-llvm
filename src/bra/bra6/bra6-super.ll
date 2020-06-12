; ModuleID = 'bra6.ll'
source_filename = "../../../code/bra6.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @fun(i32 %a, i32 %b) #0 {
entry:
  %0 = icmp sgt i32 %b, 20
  %smax = select i1 %0, i32 %b, i32 20
  %1 = add nuw i32 %smax, 1
  %2 = sub i32 %1, %b
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %b.addr.0 = phi i32 [ %b, %entry ], [ %inc, %for.inc ]
  %cmp = icmp slt i32 %b.addr.0, 20
  br i1 %cmp, label %for.inc, label %for.end

for.inc:                                          ; preds = %for.cond
  %inc = add nsw i32 %b.addr.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %3 = mul i32 %2, %a
  ret i32 %3
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
