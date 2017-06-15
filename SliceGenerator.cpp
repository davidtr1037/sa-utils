#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

#include "llvm/analysis/PointsTo/PointsTo.h"

#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"
#include "SVFPointerAnalysis.h"
#include "Slicer.h"
#include "SliceGenerator.h"

using namespace std;
using namespace llvm;
using namespace dg;

void SliceGenerator::generate() {
    /* Notes:
       - we need the nodes of the whole program
       - field sensitive (not sure if this flag changes anything...)
    */
    LLVMPointerAnalysis *llvmpta = new LLVMPointerAnalysis(module, UNKNOWN_OFFSET, "main");
    llvmpta->PS->setRoot(llvmpta->builder->buildLLVMPointerSubgraph());

    /* translate the results of SVF to DG */
    SVFPointerAnalysis svfpa(module, llvmpta, aa);
    svfpa.run();

    ModRefAnalysis::SideEffects &sideEffects = mra->getSideEffects();

    for (ModRefAnalysis::SideEffects::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        ModRefAnalysis::SideEffect &sideEffect = *i;

        ModRefAnalysis::SideEffectType type = sideEffect.type;
        Function *f = sideEffect.getFunction();
        uint32_t sliceId = sideEffect.id;

        /* set criterion functions */
        std::vector<std::string> criterions;
        if (type == ModRefAnalysis::ReturnValue) {
            criterions.push_back("ret");
        } else {
            std::set<std::string> &fnames = annotator->getAnnotatedNames(sliceId);
            for (std::set<std::string>::iterator i = fnames.begin(); i != fnames.end(); i++) {
                std::string fname = *i;
                criterions.push_back(fname);
            }
        }

        /* generate slice */
        string entryName = f->getName().data();
        Slicer *slicer = new Slicer(module, 0, entryName, criterions, llvmpta, cloner);
        slicer->setSliceId(sliceId);
        slicer->run();

        /* TODO: what happens with allocated instructions? (ret/branch/...) */
        delete slicer;
    }
}

void SliceGenerator::dumpSlices() {
    ModRefAnalysis::SideEffects &sideEffects = mra->getSideEffects();

    for (ModRefAnalysis::SideEffects::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        dumpSlices(*i);
    }
}

void SliceGenerator::dumpSlices(ModRefAnalysis::SideEffect &sideEffect) {
    Function *f = sideEffect.getFunction();
    uint32_t id = sideEffect.id;

    set<Function *> reachable = cloner->getReachabilityMap()[f];
    for (set<Function *>::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        dumpSlice(f, id);
    }
}

void SliceGenerator::dumpSlice(Function *f, uint32_t sliceId) {
    Cloner::SliceInfo *si = cloner->getSlice(f, sliceId);
    Function *sliced = si->first;
    sliced->print(outs());
}
