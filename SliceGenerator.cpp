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
            criterions.push_back(Annotator::getAnnotatedName(sliceId));
        }

        /* generate slice */
        Slicer *slicer = new Slicer(module, 0, aa, cloner, cloner->entryName, criterions);
        slicer->setSliceId(sliceId);
        slicer->run();

        /* TODO: what happens with allocated instructions? (ret/branch/...) */
        delete slicer;
    }
}

void SliceGenerator::dumpSlice(uint32_t sliceId) {
    errs() << "slice: " << sliceId << "\n";
    for (std::set<Function *>::iterator i = cloner->reachable.begin(); i != cloner->reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        Cloner::SliceInfo *si = cloner->getSlice(f, sliceId);
        Function *sliced = si->first;
        sliced->dump();
    }
}

void SliceGenerator::dumpSlices() {
    for (ModRefAnalysis::SliceIds::iterator i = mra->sliceIds.begin(); i != mra->sliceIds.end(); i++) {
        uint32_t sliceId = *i;
        dumpSlice(sliceId);
    }
}
