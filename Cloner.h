#ifndef CLONER_H
#define CLONER_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "ReachabilityAnalysis.h"
#include "ModRefAnalysis.h"

class Cloner {
public:
    typedef std::map<llvm::Value *, llvm::Value *> ValueTranslationMap;
    typedef std::pair<llvm::Function *, llvm::ValueToValueMapTy *> SliceInfo;
    typedef std::map<uint32_t, SliceInfo> SliceMap;
    typedef std::map<llvm::Function *, SliceMap> FunctionMap;
    typedef std::map<llvm::Function *, ValueTranslationMap *> CloneInfoMap;

    Cloner(llvm::Module *module, ReachabilityAnalysis *ra, ModRefAnalysis *mra) :
        module(module), ra(ra), mra(mra)
    {

    }

    ~Cloner();

    void clone(std::string name);

    void cloneFunction(llvm::Function *f, uint32_t sliceId);

    ValueTranslationMap *buildReversedMap(llvm::ValueToValueMapTy *vmap);

    SliceMap *getSlices(llvm::Function *function);

    SliceInfo *getSlice(llvm::Function *function, uint32_t sliceId);

    ValueTranslationMap *getCloneInfo(llvm::Function *cloned);
    
    llvm::Value *translateValue(llvm::Value *);

    std::set<llvm::Function *> reachable;

    FunctionMap functionMap;

    CloneInfoMap cloneInfoMap;

private:

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    ModRefAnalysis *mra;
};

#endif
