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
    /* notes:
       - UNKNOWN_OFFSET: field sensitive (not sure if this flag changes anything...)
       - main: we need the nodes of the whole program
    */
    llvmpta = new LLVMPointerAnalysis(module, UNKNOWN_OFFSET, "main");
    llvmpta->PS->setRoot(llvmpta->builder->buildLLVMPointerSubgraph());

    /* translate the results of SVF to DG */
    SVFPointerAnalysis svfpa(module, llvmpta, aa);
    svfpa.run();

    if (lazyMode) {
        return;
    }

    /* generate all the slices... */
    ModRefAnalysis::SideEffects &sideEffects = mra->getSideEffects();
    for (ModRefAnalysis::SideEffects::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        generateSlice(i->getFunction(), i->id, i->type);
    }
}

void SliceGenerator::generateSlice(Function *f, uint32_t sliceId, ModRefAnalysis::SideEffectType type) {
    std::vector<std::string> criterions;
    std::set<std::string> fnames;

    /* set criterion functions */
    switch (type) {
    case ModRefAnalysis::ReturnValue:
        criterions.push_back("ret");
        break;

    case ModRefAnalysis::Modifier:
        fnames = annotator->getAnnotatedNames(sliceId);
        for (std::set<std::string>::iterator i = fnames.begin(); i != fnames.end(); i++) {
            std::string fname = *i;
            criterions.push_back(fname);
        }
        break;

    default:
        assert(false);
        break;
    }

    /* generate slice */
    string entryName = f->getName().data();
    Slicer *slicer = new Slicer(module, 0, entryName, criterions, llvmpta, cloner);
    slicer->setSliceId(sliceId);
    slicer->run();

    markAsSliced(f, sliceId);
}

void SliceGenerator::markAsSliced(Function *sliceEntry, uint32_t sliceId) {
    /* mark all reachable functions as sliced... */
    set<Function *> &reachable = cloner->getReachabilityMap()[sliceEntry];
    for (set<Function *>::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        Cloner::SliceInfo *sliceInfo = cloner->getSliceInfo(*i, sliceId);
        sliceInfo->isSliced = true;
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

    set<Function *> &reachable = cloner->getReachabilityMap()[f];
    for (set<Function *>::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        dumpSlice(f, id);
    }
}

void SliceGenerator::dumpSlice(Function *f, uint32_t sliceId) {
    Cloner::SliceInfo *si = cloner->getSliceInfo(f, sliceId);
    Function *sliced = si->f;
    sliced->print(outs());
}
