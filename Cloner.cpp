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

#include "ModRefAnalysis.h"
#include "Cloner.h"

using namespace llvm;

void Cloner::clone(std::string name) {
    Function *entry = module->getFunction(StringRef(name));

    errs() << "creating " << mra->sliceIds.size() << " slices\n";

    for (ModRefAnalysis::SliceIds::iterator i = mra->sliceIds.begin(); i != mra->sliceIds.end(); i++) {
        uint32_t sliceId = *i;

        /* TODO: check the last parameter! */
        ValueToValueMapTy *vmap = new ValueToValueMapTy();
        Function *cloned = CloneFunction(entry, *vmap, true);

        /* set function name */
        std::string clonedName = name + std::string("_clone_") + std::to_string(sliceId);
        cloned->setName(StringRef(clonedName));

        /* update map */
        SliceInfo sliceInfo = std::make_pair(cloned, vmap);
        functionMap[entry][sliceId] = sliceInfo;

        /* update map */
        cloneInfoMap[cloned] = buildReversedMap(vmap);
    }

    //for (inst_iterator i = inst_begin(entry); i != inst_end(entry); i++) {
    //    Instruction *inst = &*i;
    //    Value *v = dyn_cast<Value>(inst);
    //    WeakVH &wvh = vmap[v];
    //    Value *nv = &*wvh;
    //    errs() << "old: " << v << " new: " << nv << "\n";
    //    v->dump();
    //    nv->dump();
    //    if (inst->getOpcode() == Instruction::Load) {
    //        LoadInst *load = dyn_cast<LoadInst>(v);
    //        LoadInst *nload = dyn_cast<LoadInst>(nv);
    //        Value *ptr = load->getPointerOperand();
    //        Value *nptr = nload->getPointerOperand();
    //        errs() << "old ptr: " << ptr << "\n"; ptr->dump(); 
    //        errs() << "new ptr: " << nptr << "\n"; nptr->dump(); 
    //    }
    //    if (inst->getOpcode() == Instruction::Call) {
    //        CallInst *call = dyn_cast<CallInst>(v);
    //        CallInst *ncall = dyn_cast<CallInst>(nv);
    //        Value *called = call->getCalledValue();
    //        Value *ncalled = ncall->getCalledValue();
    //        errs() << "old call: " << called << "\n";
    //        errs() << "new call: " << ncalled << "\n";
    //    }
    //}
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
    FunctionMap::iterator entry = functionMap.find(function);
    if (entry == functionMap.end()) {
        return 0;
    }

    return &(entry->second);
}

Cloner::SliceInfo *Cloner::getSlice(llvm::Function *function, uint32_t sliceId) {
    SliceMap *sliceMap = getSlices(function);
    if (!sliceMap) {
        return 0;
    }

    SliceMap::iterator entry = sliceMap->find(sliceId);
    if (entry == sliceMap->end()) {
        return 0;
    }

    return &(entry->second);
}

Cloner::ValueTranslationMap *Cloner::getCloneInfo(llvm::Function *cloned) {
    CloneInfoMap::iterator i = cloneInfoMap.find(cloned);
    if (i == cloneInfoMap.end()) {
        return 0;
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
