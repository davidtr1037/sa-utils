#ifndef SLICEGENERATOR_H
#define SLICEGENERATOR_H

#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"

class SliceGenerator {
public:

    SliceGenerator(
        llvm::Module *module,
        AAPass *aa,
        ModRefAnalysis *mra,
        Annotator *annotator,
        Cloner *cloner
    ) :
        module(module), 
        aa(aa), 
        mra(mra), 
        annotator(annotator),
        cloner(cloner),
        llvmpta(0)
    {

    }

    void generate();

    void generateSlice(llvm::Function *f, uint32_t sliceId, ModRefAnalysis::SideEffectType type);

    void dumpSlices();

private:

    void dumpSlices(ModRefAnalysis::SideEffect &sideEffect);

    void dumpSlice(llvm::Function *f, uint32_t sliceId);

    llvm::Module *module;
    AAPass *aa;
    ModRefAnalysis *mra;
    Annotator *annotator;
    Cloner *cloner;
    LLVMPointerAnalysis *llvmpta;
};

#endif
