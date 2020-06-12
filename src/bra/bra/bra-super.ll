; ModuleID = 'bra.ll'
source_filename = "../../../code/bra.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
entry:
  br label %while.cond

while.cond:                                       ; preds = %for.end, %entry
  %k.0 = phi i32 [ 0, %entry ], [ %add2, %for.end ]
  %exitcond1 = icmp eq i32 %k.0, 20
  br i1 %exitcond1, label %while.end, label %for.cond

for.cond:                                         ; preds = %while.cond, %for.inc
  %j.0 = phi i32 [ %inc, %for.inc ], [ 0, %while.cond ]
  %exitcond = icmp eq i32 %j.0, 10
  br i1 %exitcond, label %for.end, label %for.inc

for.inc:                                          ; preds = %for.cond
  %inc = add nuw nsw i32 %j.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %add2 = add nuw nsw i32 %k.0, 1
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i32 20) #2
  ret i32 601
}

declare dso_local i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.1.0 "}
