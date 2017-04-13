#include <stdbool.h>
#include <iostream>

#include <llvm/IR/Module.h>

#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"
#include "Slicer.h"
#include "SliceGenerator.h"

using namespace llvm;

void SliceGenerator::generate() {
    for (ModRefAnalysis::SliceIds::iterator i = mra->sliceIds.begin(); i != mra->sliceIds.end(); i++) {
        uint32_t sliceId = *i;

        /* set criterion function */
        std::vector<std::string> criterions;
        if (sliceId == mra->retSliceId) {
            criterions.push_back("ret");
        } else {
            std::set<std::string> &fnames = annotator->getAnnotatedNames(sliceId);
            for (std::set<std::string>::iterator i = fnames.begin(); i != fnames.end(); i++) {
                std::string fname = *i;
                criterions.push_back(fname);
            }
        }

        /* generate slice */
        Slicer *slicer = new Slicer(module, 0, aa, cloner, cloner->entryName, criterions);
        slicer->setSliceId(sliceId);
        slicer->run();

        /* TODO: what happens with allocated instructions? (ret/branch/...) */
        delete slicer;
    }
}

void SliceGenerator::dumpSlices(Function *f) {
    f->dump();
    for (ModRefAnalysis::SliceIds::iterator i = mra->sliceIds.begin(); i != mra->sliceIds.end(); i++) {
        uint32_t sliceId = *i;
        Cloner::SliceInfo *si = cloner->getSlice(f, sliceId);
        Function *sliced = si->first;
        sliced->dump();
    }
}

void SliceGenerator::dumpSlices() {
    for (std::set<Function *>::iterator i = cloner->reachable.begin(); i != cloner->reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        dumpSlices(f);
    }
}
