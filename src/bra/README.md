## Generate SSA from using -mem2reg
- Create file `bra.c` in directory **~/Public/project/code**
- Open directory **~/Public/project/llvm-project/build/bin**
- Run command `./clang -c -O0 -emit-llvm ../../../code/bra.c -o bra.bc -Xclang -disable-O0-optnone` to generate `bra.bc` (bitecode)
- Run command `./llvm-dis bra.bc -o=bra.ll` to generate `bra.ll` (IR)
- Run command `./opt -mem2reg -S bra.ll -o bra-opt.ll` to generate SSA form using `-mem2reg` (Remove instructions and add PHI nodes1)