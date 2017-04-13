#ifndef SLICEGENERATOR_H
#define SLICEGENERATOR_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>

#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"

class SliceGenerator {
public:

    SliceGenerator(llvm::Module *module, AAPass *aa, ModRefAnalysis *mra, Annotator *annotator, Cloner *cloner) :
        module(module), 
        aa(aa), 
        mra(mra), 
        annotator(annotator),
        cloner(cloner)
    {

    }

    void generate();

    void dumpSlices(llvm::Function *f);

    void dumpSlices();

    llvm::Module *module;
    AAPass *aa;
    ModRefAnalysis *mra;
    Annotator *annotator;
    Cloner *cloner;
};

#endif
