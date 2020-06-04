## Generate SSA from using -mem2reg
- Create file `bra.c` in directory **~/Public/project/code**
- Open directory **~/Public/project/llvm-project/build/bin**
- Run command `./clang -c -O0 -emit-llvm ../../../code/bra.c -o bra.bc -Xclang -disable-O0-optnone` to generate `bra.bc` (bitecode)
- Run command `./llvm-dis bra.bc -o=bra.ll` to generate `bra.ll` (IR)
- Run command `./opt -mem2reg -S bra.ll -o bra-opt.ll` to generate SSA form using `-mem2reg` (Remove instructions and add PHI nodes1)

## Supported instructions
- `a = b + 1`
- `a = 1 + b`
- `a = 1 + 1`
- `a < 1`
- `1 < a`
- `a <= 1`
- `1 <= a`
- `a > 1`
- `1 > a`
- `a >= 1`
- `1 >= a`
- Phi of constant and reference
- Phi of two references
- `a > 1 && a < 5` (still to verify)

Note: Since we use mem2reg optimization, comparison of two constant values (e.g. `1 < 10`) are not possible in the code (pruned/optimized)

## Not supported instructions
- `a < b`
- `a <= b`
- `a > b`
- `a >= b`
- All comparisons different from `<`, `>`, `<=`, `>=` on signed integers
- `a + b`
- `a - b`
- All operations different from `+` and `-`
- Only integer values (no floating point)
- Phi of two constant