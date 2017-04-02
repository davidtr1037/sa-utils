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
    typedef std::map<llvm::Value *, llvm::Value *> ValueTranslationMap;
    typedef std::pair<llvm::Function *, llvm::ValueToValueMapTy *> SliceInfo;
    typedef std::map<uint32_t, SliceInfo> SliceMap;
    typedef std::map<llvm::Function *, SliceMap> FunctionMap;
    typedef std::map<llvm::Function *, ValueTranslationMap *> CloneInfoMap;

    Cloner(llvm::Module *module, ModRefAnalysis *mra) :
        module(module), mra(mra)
    {

    }

    ~Cloner();

    void clone(std::string name);

    ValueTranslationMap *buildReversedMap(llvm::ValueToValueMapTy *vmap);

    SliceMap *getSlices(llvm::Function *function);

    SliceInfo *getSlice(llvm::Function *function, uint32_t sliceId);

    ValueTranslationMap *getCloneInfo(llvm::Function *cloned);
    
    llvm::Value *translateValue(llvm::Value *);

    FunctionMap functionMap;

    CloneInfoMap cloneInfoMap;

private:

    llvm::Module *module;
    ModRefAnalysis *mra;
};

#endif
