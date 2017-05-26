#include <stdbool.h>
#include <iostream>
#include <set>
#include <map>

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

using namespace std;
using namespace llvm;

Cloner::Cloner(llvm::Module *module, ReachabilityAnalysis *ra, ModRefAnalysis *mra) :
    module(module),
    ra(ra),
    mra(mra),
    targets(mra->getTargets())
{

}

void Cloner::run() {
    ModRefAnalysis::SideEffects &sideEffects = mra->getSideEffects();

    for (ModRefAnalysis::SideEffects::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        ModRefAnalysis::SideEffect &sideEffect = *i;
        Function *f = sideEffect.getFunction();
        uint32_t sliceId = sideEffect.id;

        /* compute reachable functions only once */
        if (reachabilityMap.find(f) == reachabilityMap.end()) {
            set<Function *> &reachable = reachabilityMap[f];
            ra->computeReachableFunctions(f, reachable);
            outs() << f->getName() << ": " << reachable.size() << " reachable functions\n";
        }

        set<Function *> &cached = reachabilityMap[f];
        for (set<Function *>::iterator j = cached.begin(); j != cached.end(); j++) {
            Function *f = *j;
            if (f->isDeclaration()) {
                continue;
            }

            outs() << "cloning: " << f->getName() << "\n";
            cloneFunction(f, sliceId);
        }
    }
}

void Cloner::cloneFunction(Function *f, uint32_t sliceId) {
    /* TODO: check the last parameter! */
    ValueToValueMapTy *vmap = new ValueToValueMapTy();
    Function *cloned = CloneFunction(f, *vmap, true);

    /* set function name */
    string clonedName = string(f->getName().data()) + string("_clone_") + to_string(sliceId);
    cloned->setName(StringRef(clonedName));

    /* update map */
    SliceInfo sliceInfo = make_pair(cloned, vmap);
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
        map->insert(make_pair(mappedValue, value));
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
        /* TODO: add assert instead? */
        return NULL;
    }
    return i->second;
}

Cloner::ReachabilityMap &Cloner::getReachabilityMap() {
    return reachabilityMap;
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
