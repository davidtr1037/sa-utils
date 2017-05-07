# Static Slicing Project
Developed for the KLEE/Slicing project.

Provides:
* Mod/Ref Analysis
* Static Slicing (with support for multiple slicing)

## Build
Build the SVF project (https://github.com/davidtr1037/SVF/tree/llvm-3.4).
Build the DG project (https://github.com/davidtr1037/dg/tree/extended-slicing).

```
make \\
    LLVM_SRC=<LLVM_SRC_DIR> \
    LLVM_OBJ=<LLVM_OBJ_DIR> \
    SVF_PATH=<SVF_ROOT_DIR> \
    DG_PATH=<DG_ROOT_DIR> \
    all
```

Note: We assume that LLVM was built with autoconf.
