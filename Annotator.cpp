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
    /* annotate return's... */
    Function *f = module->getFunction(mra->target);
    annotateReturns(f, mra->retSliceId);

    for (ModRefAnalysis::AllocSiteToStoreMap::iterator i = mra->allocSiteToStoreMap.begin(); i != mra->allocSiteToStoreMap.end(); i++) {
        std::pair<const Value *, uint64_t> key = i->first;
        std::set<Instruction *> stores = i->second;
        uint32_t sliceId = mra->allocSiteToIdMap[key];
        annotateStores(stores, sliceId);
    }
}

void Annotator::annotateReturns(Function *f, uint32_t sliceId) {
    if (f->getReturnType()->isVoidTy()) {
        errs() << "No return value, skipping return annotations...\n";
        return;
    }

    for (inst_iterator i = inst_begin(f); i != inst_end(f); i++) {
        Instruction *inst = &*i;
        if (inst->getOpcode() == Instruction::Ret) {
            annotateReturn(inst, sliceId);
        }
    }
}

void Annotator::annotateReturn(Instruction *inst, uint32_t sliceId) {
    ReturnInst *returnInst = dyn_cast<ReturnInst>(inst);

    /* generate a unique function name */
    std::string fname = getAnnotatedName(sliceId);

    /* create criterion function */
    Value *returnValue = returnInst->getReturnValue();
    module->getOrInsertFunction(
        fname,
        Type::getVoidTy(module->getContext()), 
        returnValue->getType(),
        NULL
    );
    Function *criterionFunction = module->getFunction(fname);

    /* insert call */
    std::vector<Value* > args;
    args.push_back(returnValue);
    CallInst *callInst = CallInst::Create(criterionFunction, args, "");
    callInst->insertBefore(returnInst);
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
