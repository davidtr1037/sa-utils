#ifndef SLICEGENERATOR_H
#define SLICEGENERATOR_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>

#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Cloner.h"

class SliceGenerator {
public:

    SliceGenerator(llvm::Module *module, AAPass *aa, ModRefAnalysis *mra, Cloner *cloner) :
        module(module), aa(aa), mra(mra), cloner(cloner)
    {

    }

    void generate();

    void dumpSlice(uint32_t sliceId);

    void dumpSlices();

private:

    llvm::Module *module;
    AAPass *aa;
    ModRefAnalysis *mra;
    Cloner *cloner;
};

#endif
