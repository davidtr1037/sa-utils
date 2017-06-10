LLVM_OBJ=/home/david/tau/llvm-3.4.obj
LLVM_BIN_DIR=$(LLVM_OBJ)/bin
CLANG=$(LLVM_BIN_DIR)/clang
OPT=$(LLVM_BIN_DIR)/opt
LLVM_DIS=$(LLVM_BIN_DIR)/llvm-dis
LLVM_LINK=$(LLVM_BIN_DIR)/llvm-link
KLEE_PATH=/home/david/tau/klee/klee

SOURCES=$(shell ls *.c)

TARGET=final.bc
all: $(TARGET)

%.bc: %.c
	$(CLANG) -c -g -emit-llvm -I$(KLEE_PATH)/include $< -o $@

$(TARGET): $(patsubst %.c,%.bc,$(SOURCES))
	$(LLVM_LINK) $^ -o $@
	$(OPT) -mem2reg $@ -o $@
	$(LLVM_DIS) $@

clean:
	rm -f *.bc *.ll
	rm -rf klee-out-* klee-last
