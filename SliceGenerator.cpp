#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"
#include "Slicer.h"
#include "SliceGenerator.h"

using namespace std;
using namespace llvm;

void SliceGenerator::generate() {
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
        Slicer *slicer = new Slicer(module, 0, aa, cloner, entryName, criterions);
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
