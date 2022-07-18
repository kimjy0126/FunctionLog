# FunctionLog
FunctionLog is LLVM instrument pass for logging every function entry and return.

`CMakeLists.txt` file is from [here](https://github.com/banach-space/llvm-tutor)

### How to build
Requirement: LLVM 15

```
mkdir build
cd build
cmake ..
make
```

### Usage
```
opt -load-pass-plugin ./libFunctionLog.so -passes="fnlog" <IR file> -S >output.ll
llc -filetype=obj output.ll -o output.o
clang -o output output.o
./output
```
