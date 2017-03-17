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
        id = 0;
        argId = 0;
    }

    void annotate();
    void annotateStore(llvm::Instruction *inst);

private:

    llvm::Module *module;
    ModRefAnalysis *mra;
    uint32_t id;
    uint32_t argId;
};

#endif
