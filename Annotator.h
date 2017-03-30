#ifndef ANNOTATOR_H
#define ANNOTATOR_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

#include "ModRefAnalysis.h"

class Annotator {
public:

    Annotator(llvm::Module *module, ModRefAnalysis *mra) :
        module(module), mra(mra)
    {
        argId = 0;
    }

    void annotate();

    void annotateReturns(llvm::Function *f, uint32_t sliceId);

    void annotateReturn(llvm::Instruction *inst, uint32_t sliceId);

    void annotateStores(std::set<llvm::Instruction *> &stores, uint32_t sliceId);

    void annotateStore(llvm::Instruction *inst, uint32_t sliceId);

    static std::string getAnnotatedName(uint32_t id);

private:

    llvm::Module *module;
    ModRefAnalysis *mra;
    uint32_t argId;
};

#endif
