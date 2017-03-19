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

    unsigned int nslices = mra->allocSiteToStoreMap.size();
    errs() << "creating " << nslices << " slices\n";

    for (unsigned int i = 0; i < nslices; i++) {
        ValueToValueMapTy *vmap = new ValueToValueMapTy();
        Function *cloned = CloneFunction(entry, *vmap, true);
        uint32_t sliceId = i + 1;
        functionMap[entry][sliceId] = std::make_pair(cloned, vmap);
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
