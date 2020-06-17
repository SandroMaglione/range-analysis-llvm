## Generate SSA from using -mem2reg
- Create file `bra.c` in directory **~/Public/project/code**
- Open directory **~/Public/project/llvm-project/build/bin**
- Run command `./clang -c -O0 -emit-llvm ../../../code/example.c -o example.bc -Xclang -disable-O0-optnone` to generate `bra.bc` (bitecode)
- Run command `./llvm-dis example.bc -o=example.ll` to generate `bra.ll` (IR)
- Run command `./opt -mem2reg -constprop -dce -simplifycfg -gvn -S example.ll -o example-super.ll` to generate SSA form using `-mem2reg` (Remove instructions and add PHI nodes)

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

## Not supported instructions
- All operations different from `+` and `-`
- Only integer values (no floating point)