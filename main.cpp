#include <stdio.h>
#include <iostream>

#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Support/CommandLine.h>

#include <WPA/Andersen.h>
#include <MemoryModel/PointerAnalysis.h>

#include "ReachabilityAnalysis.h"
#include "Inliner.h"
#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"
#include "Slicer.h"
#include "SliceGenerator.h"

using namespace std;
using namespace llvm;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: <bitcode-file> <sliced-function-1> <sliced-function-2> ... \n");
        return 1;
    }

    string inputFile(argv[1]);
    Module *module;
    SMDiagnostic err;

    LLVMContext &context = getGlobalContext();
    module = ParseIRFile(inputFile, err, context);
    if (!module) {
        return 1;
    }

    vector<string> targets;
    for (unsigned int i = 2; i < argc; i++) {
        std::string slicedFunction = std::string(argv[i]);
        if (!module->getFunction(slicedFunction)) {
            fprintf(stderr, "Sliced function '%s' not found...\n", argv[i]);
            return 1;
        }
        targets.push_back(slicedFunction);
    }

    vector<string> inlineTargets;

    ReachabilityAnalysis *ra = new ReachabilityAnalysis(module);
    Inliner *inliner = new Inliner(module, ra, targets, inlineTargets);
    AAPass *aa = new AAPass();
    aa->setPAType(PointerAnalysis::Andersen_WPA);
    ModRefAnalysis *mra = new ModRefAnalysis(module, ra, aa, "main", targets);
    Annotator *annotator = new Annotator(module, mra);
    Cloner *cloner = new Cloner(module, ra, mra);
    SliceGenerator *sg = new SliceGenerator(module, aa, mra, annotator, cloner);

    ra->run();
    inliner->run();
    legacy::PassManager pm;
    pm.add(aa);
    pm.run(*module);
    mra->run();
    annotator->annotate();
    cloner->run();
    sg->generate();
    sg->dumpSlices();

    return 0;
}
