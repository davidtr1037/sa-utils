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
    unsigned int nslices = mra->allocSiteToStoreMap.size();
    for (unsigned int i = 0; i < nslices; i++) {
        /* TODO: ... */
        uint32_t sliceId = i + 1;

        /* set criterion function */
        std::vector<std::string> criterions;
        criterions.push_back(Annotator::getAnnotatedName(sliceId));

        /* generate slice */
        Slicer *slicer = new Slicer(module, 0, aa, cloner, target, criterions);
        slicer->setSliceId(sliceId);
        slicer->run();

        /* TODO: what happens with allocated instructions? (ret/branch/...) */
        delete slicer;
    }
}

void SliceGenerator::dumpSlice(uint32_t sliceId) {
    Cloner::SliceInfo *si = cloner->getSlice(module->getFunction(StringRef("f")), sliceId);
    Function *sliced = si->first;   
    errs() << "slice: " << sliceId << "\n";
    sliced->dump();
}

void SliceGenerator::dumpSlices() {
    unsigned int nslices = mra->allocSiteToStoreMap.size();
    for (unsigned int i = 0; i < nslices; i++) {
        dumpSlice(i + 1);
    }
}
