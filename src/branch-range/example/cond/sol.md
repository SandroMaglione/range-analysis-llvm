**entry**

**while.cond**
phi
MISSING %a.1

phi
MISSING %add3

cmp + br
%while.body -> %k.0 (-Inf, 9)
%while.end -> %k.0 (10, Inf)

**while.body**
%k.0 (-Inf, 9)

add
%add (-Inf, Inf)

add
%add2 (-Inf, Inf)

add
%add3 (-Inf, 10)

br
%while.cond
%k.0 (-Inf, 9)
%add (-Inf, Inf)
%add2 (-Inf, Inf)
%add3 (-Inf, 10)

**while.end**
%k.0 (10, Inf)

**while.cond**
%k.0 (-Inf, 9)
%add (-Inf, Inf)
%add2 (-Inf, Inf)
%add3 (-Inf, 10)

phi
MISSING %a.1

phi
%k.0 (0, 9)

cmp + br
%while.body -> %k.0 (0, 9)
%while.end -> %k.0 (10, 10)

**while.body**
%k.0 (0, 9)

add
%add SAME

add
%add2 SAME

add
%add3 (1, 10)

br
%while.cond
%k.0 (0, 9)
%add3 (1, 10)

**while.end**
%k.0 (10, 10)

**while.cond**
%k.0 (0, 9)
%add (-Inf, Inf)
%add2 (-Inf, Inf)
%add3 (1, 10)

phi
MISSING %a.1

phi
%k.0 (0, 9)

cmp + br
%while.body -> %k.0 SAME
%while.end -> %k.0 SAME




