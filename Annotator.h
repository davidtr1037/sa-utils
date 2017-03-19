#ifndef ANNOTATOR_H
#define ANNOTATOR_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>

#include "ModRefAnalysis.h"

class Annotator {
public:

    Annotator(llvm::Module *module, ModRefAnalysis *mra) :
        module(module), mra(mra)
    {
        sliceId = 1;
        argId = 1;
    }

    void annotate();

    void annotateStores(std::set<llvm::Instruction *> &stores);

    void annotateStore(llvm::Instruction *inst);

    static std::string getAnnotatedName(uint32_t id);

private:

    llvm::Module *module;
    ModRefAnalysis *mra;
    uint32_t sliceId;
    uint32_t argId;
};

#endif
