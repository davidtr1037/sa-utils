LLVM_SRC_PATH=/home/david/tau/llvm-3.4.src
LLVM_OBJ_PATH=/home/david/tau/llvm-3.4.obj
LLVM_CONFIG=$(LLVM_OBJ_PATH)/Release+Asserts/bin/llvm-config
WPA_PATH=/home/david/tau/SVF-port-3.4
DG_PATH=/home/david/tau/dg/

LLVM_LIBS=$(shell $(LLVM_CONFIG) --libs)
LLVM_LDFLAGS=$(shell $(LLVM_CONFIG) --ldflags)
CXX=g++ -m32

INCLUDES=\
    -I$(LLVM_SRC_PATH)/include/ \
    -I$(LLVM_OBJ_PATH)/include/ \
    -I$(WPA_PATH)/include \
    -I$(DG_PATH)/src \
    -I$(DG_PATH)/tools

CXXFLAGS=$(INCLUDES) -DHAVE_LLVM -DENABLE_CFG -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -std=gnu++11 -g -fno-rtti

DEP_LIBS=\
    $(WPA_PATH)/Release+Asserts/lib/libwpa.so \
    $(WPA_PATH)/Release+Asserts/lib/libmssa.so \
    $(DG_PATH)/build/src/libLLVMdg.so \
    $(DG_PATH)/build/src/libLLVMpta.so \
    $(DG_PATH)/build/src/libPTA.so \
    $(DG_PATH)/build/src/libRD.so \
    $(LLVM_LIBS) 

LDFLAGS=-L$(DG_PATH)/build/src $(DEP_LIBS) $(LLVM_LDFLAGS)

SOURCES=\
		ReachabilityAnalysis.cpp \
		AAPass.cpp \
		ModRefAnalysis.cpp \
		SVFPointerAnalysis.cpp \
        Slicer.cpp \
		main.cpp

DEPS=$(patsubst %.cpp,%.o,$(SOURCES))
TARGET=main

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TARGET): $(DEPS)
	$(CXX) $^ -o $@ $(LDFLAGS) 

all: $(TARGET)

clean:
	rm -rf $(DEPS) $(TARGET)
