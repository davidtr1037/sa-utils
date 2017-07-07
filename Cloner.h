#ifndef CLONER_H
#define CLONER_H

#include <stdbool.h>
#include <iostream>
#include <set>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "ReachabilityAnalysis.h"
#include "ModRefAnalysis.h"

class Cloner {
public:

    typedef std::map<llvm::Value *, llvm::Value *> ValueTranslationMap;
    struct SliceInfo {
        /* the cloned/sliced function */
        llvm::Function *f;
        /* we have to know if it was already sliced */
        bool isSliced;
        /* translates an original value to a cloned one */
        llvm::ValueToValueMapTy *v2vmap;
    };
    typedef std::map<uint32_t, SliceInfo> SliceMap;
    typedef std::map<llvm::Function *, SliceMap> FunctionMap;
    typedef std::map<llvm::Function *, ValueTranslationMap *> CloneInfoMap;
    typedef std::set<llvm::Function *> FunctionSet;
    typedef std::map<llvm::Function *, FunctionSet> ReachabilityMap;

    Cloner(llvm::Module *module, ReachabilityAnalysis *ra);

    ~Cloner();

    void clone(llvm::Function *f, uint32_t sliceId);

    SliceMap *getSlices(llvm::Function *function);

    SliceInfo *getSliceInfo(llvm::Function *function, uint32_t sliceId);

    llvm::Value *translateValue(llvm::Value *);

    bool getReachableFunctions(
        llvm::Function *f,
        std::set<llvm::Function *> &functions
    );

private:

    void cloneFunction(llvm::Function *f, uint32_t sliceId);

    ValueTranslationMap *buildReversedMap(llvm::ValueToValueMapTy *vmap);

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    FunctionMap functionMap;
    CloneInfoMap cloneInfoMap;
    ReachabilityMap reachabilityMap;
};

#endif
