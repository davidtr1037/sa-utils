LLVM_OBJ=/home/david/tau/llvm-3.4.obj
CLANG=$(LLVM_OBJ)/bin/clang
OPT=$(LLVM_OBJ)/bin/opt
LLVM_DIS=$(LLVM_OBJ)/bin/llvm-dis
LLVM_LINK=$(LLVM_OBJ)/bin/llvm-link
KLEE_PATH=/home/david/tau/klee/klee

SOURCES=$(shell ls *.c)
TARGETS=$(patsubst %.c,%.bc,$(SOURCES))

all: $(TARGETS)

%.bc: %.c
	$(CLANG) -c -g -emit-llvm -I$(KLEE_PATH)/include $< -o $@
	$(OPT) -mem2reg $@ -o $@
	$(LLVM_DIS) $@

clean:
	rm -f *.bc *.ll
	rm -rf klee-out-* klee-last
