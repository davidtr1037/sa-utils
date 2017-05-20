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
    ModRefAnalysis::ModInfoToIdMap &modInfoToIdMap = mra->getModInfoToIdMap();

    for (ModRefAnalysis::ModInfoToIdMap::iterator i = modInfoToIdMap.begin(); i != modInfoToIdMap.end(); i++) {
        ModRefAnalysis::ModInfo modInfo = i->first;
        uint32_t sliceId = i->second;
        Function *f = modInfo.first;

        /* set criterion functions */
        std::vector<std::string> criterions;
        if (sliceId == mra->getRetSliceId(f)) {
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
    ModRefAnalysis::ModInfoToIdMap &modInfoToIdMap = mra->getModInfoToIdMap();
    outs() << "dumping " << modInfoToIdMap.size() << " slices\n";

    for (ModRefAnalysis::ModInfoToIdMap::iterator i = modInfoToIdMap.begin(); i != modInfoToIdMap.end(); i++) {
        ModRefAnalysis::ModInfo modInfo = i->first;
        uint32_t id = i->second;
        dumpSlices(modInfo, id);
    }
}

void SliceGenerator::dumpSlices(ModRefAnalysis::ModInfo &modInfo, uint32_t id) {
    Function *f = modInfo.first;
    set<Function *> reachable = cloner->getReachabilityMap()[f];

    for (set<Function *>::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        if (f->isDeclaration()) {
            continue;
        }

        dumpSlice(*i, id);
    }
}

void SliceGenerator::dumpSlice(Function *f, uint32_t sliceId) {
    Cloner::SliceInfo *si = cloner->getSlice(f, sliceId);
    Function *sliced = si->first;
    sliced->print(outs());
}
