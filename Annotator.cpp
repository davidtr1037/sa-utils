#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>

#include <llvm-c/Core.h>

#include "ModRefAnalysis.h"
#include "Annotator.h"

using namespace llvm;

void Annotator::annotate() {
    for (ModRefAnalysis::AllocSiteToStoreMap::iterator i = mra->allocSiteToStoreMap.begin(); i != mra->allocSiteToStoreMap.end(); i++) {
        std::pair<const Value *, uint64_t> key = i->first;
        std::set<Instruction *> stores = i->second;
        annotateStores(stores);
        id++;
    }
}

void Annotator::annotateStores(std::set<Instruction *> &stores) {
    for (std::set<Instruction *>::iterator i = stores.begin(); i != stores.end(); i++) {
        Instruction *inst = *i;
        annotateStore(inst);
    }
}

void Annotator::annotateStore(Instruction *inst) {
    StoreInst *store = dyn_cast<StoreInst>(inst);
    Value *pointer = store->getPointerOperand();

    /* generate a unique name */
    std::string *name = new std::string(std::string("__crit_arg_") + std::to_string(argId++));
    LoadInst *loadInst = new LoadInst(pointer, name->data());
    loadInst->setAlignment(store->getAlignment());
    loadInst->insertAfter(inst);

    std::string fname = std::string("__crit_") + std::to_string(id);
    PointerType *pointerType = dyn_cast<PointerType>(pointer->getType());
    module->getOrInsertFunction(
        fname,
        Type::getVoidTy(module->getContext()), 
        pointerType->getElementType(),
        NULL
    );
    Function *criterionFunction = module->getFunction(fname);

    dyn_cast<Value>(loadInst)->dump();
    std::vector<Value* > args;
    args.push_back(dyn_cast<Value>(loadInst));
    CallInst *callInst = CallInst::Create(criterionFunction, args, "");
    callInst->insertAfter(loadInst);
    
}
