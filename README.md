## Overview
The idea is to have a programming language with macros are so powerful, they
can even call directly into the code generator.

## Example
```
include "def.dlr";
funproto puts (str a;) i32;
defun main () i32 {
  puts "Hello, world!";
};
```

## Macro Example
This is the current implementation of the `not` operator as a macro:
```
defun lower-not ((rtv a)) rtv
  (make-twin-rtv (LLVMBuildNot bldr (unwrap-llvm-value a) "not") a);
defmacro not
  (lower-not (eval (car args)));
```

This defines `not` as a macro, a function that is called at compile-time, that
is passed a list of arguments and that can generate code.

`not` does so by evaluating its first argument (read: generating run-time code
for evaluation of its first argument) and then calling into a regular function
called `lower-not` which calls `LLVMBuildNot` and generates type information
for the front-end.

## Dependencies
* Linux
* Modern C/C++ compiler
* LLVM 10+ (NOTE: LLVM 13 support is WIP)

## Compiling it
Just type `make`. You may need to specify the environment variables `CC`, `CXX` and `LLVMPREFIX`.

If you encounter any compiler errors, try running `make report` or use another version of LLVM.

## Examples
You can find some examples -- including a pong implementation and a REPL for testing --
in the example folder.
