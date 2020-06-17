## Instructions
- Create file `simple.c` (source code to analize) inside **~/Public/project/code**
- Open directory **~/Public/project/llvm-project/llvm/lib/Transforms**
- Create directory **ConstantRange**
- Update `CMakeLists.txt` inside **~/Public/project/llvm-project/llvm/lib/Transforms** by adding the newly created directory:
```
...
add_subdirectory(ConstantRange)
```
- Open `ConstantRange` folder
- Upload `ConstantRange.cpp` file
- Create `CMakeLists.txt` file
- Add make code to `CMakeLists.txt`:
```cpp
add_llvm_loadable_module( LLVMConstantRange
  ConstantRange.cpp

  PLUGIN_TOOL
  opt
  )
```
- Open directory **~/Public/project/llvm-project/build**
- Run command `make -j4` to build the pass
- Open directory **~/Public/project/llvm-project/build/bin**
- Run command `./clang -S -emit-llvm ../../../code/simple.c` to generate the LLVM IR (file `simple.ll`)
(**Note**: The source files in C are stored inside **~/Public/project/code**)
- Run command `./opt -load ../lib/LLVMConstantRange.so -const-range < simple.ll > /dev/null -debug` to run the pass on the IR and produce the range-analysis results