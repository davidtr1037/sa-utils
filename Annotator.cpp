#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/InstIterator.h>

#include <llvm-c/Core.h>

#include "ModRefAnalysis.h"
#include "Annotator.h"

using namespace llvm;

void Annotator::annotate() {
    for (ModRefAnalysis::AllocSiteToStoreMap::iterator i = mra->allocSiteToStoreMap.begin(); i != mra->allocSiteToStoreMap.end(); i++) {
        std::pair<const Value *, uint64_t> key = i->first;
        std::set<Instruction *> stores = i->second;
        uint32_t sliceId = mra->allocSiteToIdMap[key];
        annotateStores(stores, sliceId);
    }
}

void Annotator::annotateStores(std::set<Instruction *> &stores, uint32_t sliceId) {
    for (std::set<Instruction *>::iterator i = stores.begin(); i != stores.end(); i++) {
        Instruction *inst = *i;
        annotateStore(inst, sliceId);
    }
}

void Annotator::annotateStore(Instruction *inst, uint32_t sliceId) {
    StoreInst *store = dyn_cast<StoreInst>(inst);
    Value *pointer = store->getPointerOperand();

    /* generate a unique argument name */
    std::string *name = new std::string(std::string("__crit_arg_") + std::to_string(argId++));
    /* insert load */
    LoadInst *loadInst = new LoadInst(pointer, name->data());
    loadInst->setAlignment(store->getAlignment());
    loadInst->insertAfter(inst);

    /* generate a unique function name */
    std::string fname = getAnnotatedName(sliceId);

    /* create criterion function */
    PointerType *pointerType = dyn_cast<PointerType>(pointer->getType());
    module->getOrInsertFunction(
        fname,
        Type::getVoidTy(module->getContext()), 
        pointerType->getElementType(),
        NULL
    );
    Function *criterionFunction = module->getFunction(fname);

    /* insert call */
    std::vector<Value* > args;
    args.push_back(dyn_cast<Value>(loadInst));
    CallInst *callInst = CallInst::Create(criterionFunction, args, "");
    callInst->insertAfter(loadInst);
}

std::string Annotator::getAnnotatedName(uint32_t id) {
    return std::string("__crit_") + std::to_string(id);
}
