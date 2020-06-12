**entry**

**while.cond**

phi
MISSING %add3

icmp + br
%if.end -> %k.0 (0, 29) [unsigned]
%while-end -> %k.0 (30, Inf)

**if.end**
%k.0 (0, 29)

add
%add3 (2, 31)

br
%while.cond -> %add3 (2, 31)

**while.end**
%k.0 (30, Inf)

**while.cond**
%add3 (2, 31)

phi
$k.0 -> (min(0, 2), max(0, 31)) = (0, 31)

icmp + br
%if.end -> Same
%while-end -> %k.0 (30, 31)

**while.end**
%k.0 (30, 31)

