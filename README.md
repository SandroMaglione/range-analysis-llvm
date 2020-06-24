# Integer Range Analysis in LLVM
This repository contains the code for the **Integer Range Analysis** passes written for LLVM.
The project contains two passes: **ConstantRange.cpp** and **BranchRange.cpp**.

## Installation
The following instructions explain how to install and build the pass inside LLVM:
- Create file `example.c` (source code to analize) inside **~/Public/project/code**
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

## Benchmarks
The benchmarks sources are taken from the following repositories:
- https://github.com/TheAlgorithms/C
- https://github.com/xtaci/algorithms
- https://github.com/AllAlgorithms/c

## Info
The passes have been tested on some example files. The code is not guaranteed to function in all cases. The passes can be expanded to encompass more code statements. See `src/branch-range/example` and `src/constant-range/example` to view the test cases and their results.

## License
Created by **Maglione Sandro**, MIT license