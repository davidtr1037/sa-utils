check_var = \
    $(if $(value $(1)),,$(error "$(1) is not defined"))

$(call check_var,LLVM_SRC)
$(call check_var,LLVM_OBJ)
$(call check_var,SVF_PATH)
$(call check_var,DG_PATH)

LLVM_CONFIG=$(LLVM_OBJ)/Release+Asserts/bin/llvm-config
LLVM_LIBS=$(shell $(LLVM_CONFIG) --libs)
LLVM_LDFLAGS=$(shell $(LLVM_CONFIG) --ldflags)
CXX=g++ -m32

INCLUDES=\
    -I$(LLVM_SRC)/include/ \
    -I$(LLVM_OBJ)/include/ \
    -I$(SVF_PATH)/include \
    -I$(DG_PATH)/src \
    -I$(DG_PATH)/tools \
    -I.

CXXFLAGS=$(INCLUDES) -DHAVE_LLVM -DENABLE_CFG -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -std=gnu++11 -g -fno-rtti

EXTERNAL_LIBS=\
    $(SVF_PATH)/Release+Asserts/lib/libwpa.so \
    $(SVF_PATH)/Release+Asserts/lib/libmssa.so \
    $(DG_PATH)/build/src/libLLVMdg.so \
    $(DG_PATH)/build/src/libLLVMpta.so \
    $(DG_PATH)/build/src/libPTA.so \
    $(DG_PATH)/build/src/libRD.so

LDFLAGS=-L$(SVF_PATH)/Release+Asserts/lib -L$(DG_PATH)/build/src $(EXTERNAL_LIBS) $(LLVM_LIBS) $(LLVM_LDFLAGS)
LIB_LDFLAGS=-L$(SVF_PATH)/Release+Asserts/lib -L$(DG_PATH)/buILD/SRC $(EXTERNAL_LIBS) $(LLVM_LIBS) $(LLVM_LDFLAGS)

SOURCES=\
		ReachabilityAnalysis.cpp \
		AAPass.cpp \
		ModRefAnalysis.cpp \
		SVFPointerAnalysis.cpp \
        Slicer.cpp \
        Annotator.cpp \
        Cloner.cpp \
        SliceGenerator.cpp


TARGET_DEPS=$(patsubst %.cpp,%.o,$(SOURCES)) main.o
TARGET=main
LIB_TARGET_DEPS=$(patsubst %.cpp,%.o,$(SOURCES))
LIB_TARGET=libSlicing.so

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TARGET): $(TARGET_DEPS)
	$(CXX) $^ -o $@ $(LDFLAGS) 

$(LIB_TARGET): $(LIB_TARGET_DEPS)
	$(CXX) -shared $^ -o $@ $(LIB_LDFLAGS) 

all: $(TARGET) $(LIB_TARGET)

clean:
	rm -rf $(TARGET_DEPS) $(TARGET) $(LIB_TARGET_DEPS) $(LIB_TARGET)
