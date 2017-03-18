#ifndef CLONER_H
#define CLONER_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "ModRefAnalysis.h"

class Cloner {
public:
    typedef std::pair<llvm::Function *, llvm::ValueToValueMapTy *> SliceInfo;
    typedef std::map<uint32_t, SliceInfo> SliceMap;
    typedef std::map<llvm::Function *, SliceMap> FunctionMap;

    Cloner(llvm::Module *module, ModRefAnalysis *mra) :
        module(module), mra(mra)
    {

    }

    void clone(std::string name);

    SliceMap *getSlices(llvm::Function *function);

    SliceInfo *getSlice(llvm::Function *function, uint32_t sliceId);
    
    FunctionMap functionMap;

private:

    llvm::Module *module;
    ModRefAnalysis *mra;
};

#endif
