#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/ValueHandle.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "ReachabilityAnalysis.h"
#include "ModRefAnalysis.h"
#include "Cloner.h"

using namespace llvm;

void Cloner::run() {
    Function *entryFunction = module->getFunction(entryName);

    ra->computeReachableFunctions(entryFunction, reachable);
    errs() << reachable.size() << " reachable functions\n";

    errs() << "creating " << mra->sliceIds.size() << " slices\n";
    for (ModRefAnalysis::SliceIds::iterator i = mra->sliceIds.begin(); i != mra->sliceIds.end(); i++) {
        uint32_t sliceId = *i;
        for (std::set<Function *>::iterator j = reachable.begin(); j != reachable.end(); j++) {
            Function *f = *j;
            if (f->isDeclaration()) {
                continue;
            }
            errs() << "cloning: " << f->getName() << "\n";
            cloneFunction(f, sliceId);
        }
    }
}

void Cloner::cloneFunction(Function *f, uint32_t sliceId) {
    /* TODO: check the last parameter! */
    ValueToValueMapTy *vmap = new ValueToValueMapTy();
    Function *cloned = CloneFunction(f, *vmap, true);

    /* set function name */
    std::string clonedName = std::string(f->getName().data()) + std::string("_clone_") + std::to_string(sliceId);
    cloned->setName(StringRef(clonedName));

    /* update map */
    SliceInfo sliceInfo = std::make_pair(cloned, vmap);
    functionMap[f][sliceId] = sliceInfo;

    /* update map */
    cloneInfoMap[cloned] = buildReversedMap(vmap);
}

Cloner::ValueTranslationMap *Cloner::buildReversedMap(ValueToValueMapTy *vmap) {
    ValueTranslationMap *map = new ValueTranslationMap();
    for (ValueToValueMapTy::iterator i = vmap->begin(); i != vmap->end(); i++) {
        /* TODO: should be const Value... */
        Value *value = (Value *)(i->first);
        WeakVH &wvh = i->second;
        Value *mappedValue  = &*wvh;
        map->insert(std::make_pair(mappedValue, value));
    }

    return map;
}

Cloner::SliceMap *Cloner::getSlices(llvm::Function *function) {
    FunctionMap::iterator i = functionMap.find(function);
    if (i == functionMap.end()) {
        return 0;
    }

    return &(i->second);
}

Cloner::SliceInfo *Cloner::getSlice(llvm::Function *function, uint32_t sliceId) {
    SliceMap *sliceMap = getSlices(function);
    if (!sliceMap) {
        return 0;
    }

    SliceMap::iterator i = sliceMap->find(sliceId);
    if (i == sliceMap->end()) {
        return 0;
    }

    return &(i->second);
}

Cloner::ValueTranslationMap *Cloner::getCloneInfo(llvm::Function *cloned) {
    CloneInfoMap::iterator i = cloneInfoMap.find(cloned);
    if (i == cloneInfoMap.end()) {
        return 0;
    }

    return i->second;
}

Value *Cloner::translateValue(Value *value) {
    Instruction *inst = dyn_cast<Instruction>(value);
    if (!inst) {
        /* TODO: do we clone only instructions? */
        return value;
    }

    Function *f = inst->getParent()->getParent();
    CloneInfoMap::iterator entry = cloneInfoMap.find(f);
    if (entry == cloneInfoMap.end()) {
        /* the value is not contained in a cloned function */
        return value;
    }

    ValueTranslationMap *map = entry->second;
    ValueTranslationMap::iterator i = map->find(value);
    if (i == map->end()) {
        assert(false);
    }
    return i->second;
}

Cloner::~Cloner() {
    for (FunctionMap::iterator i = functionMap.begin(); i != functionMap.end(); i++) {
        SliceMap &sliceMap = i->second;
        for (SliceMap::iterator j = sliceMap.begin(); j != sliceMap.end(); j++) {
            SliceInfo &sliceInfo = j->second;
            Function *cloned = sliceInfo.first;
            delete cloned;
            ValueToValueMapTy *vmap = sliceInfo.second;
            delete vmap;
        }
    }
}
