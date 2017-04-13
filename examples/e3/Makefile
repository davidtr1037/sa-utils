LLVM_OBJ=/home/david/tau/llvm-3.4.obj
CLANG=$(LLVM_OBJ)/Release+Asserts/bin/clang
OPT=$(LLVM_OBJ)/Release+Asserts/bin/opt
LLVM_DIS=$(LLVM_OBJ)/Release+Asserts/bin/llvm-dis
LLVM_LINK=$(LLVM_OBJ)/Release+Asserts/bin/llvm-link
KLEE_PATH=/home/david/tau/klee/klee

SOURCES=\
    md5.c \
    main.c

TARGET=final.bc
all: $(TARGET)

%.bc: %.c
	$(CLANG) -m32 -c -g -emit-llvm -I$(KLEE_PATH)/include $< -o $@

$(TARGET): $(patsubst %.c,%.bc,$(SOURCES))
	$(LLVM_LINK) $^ -o $@
	$(OPT) -mem2reg $@ -o $@
	$(LLVM_DIS) $@

clean:
	rm *.bc *.ll
