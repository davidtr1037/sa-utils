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
#include "AAPass.h"
#include "ModRefAnalysis.h"
#include "Annotator.h"
#include "Cloner.h"
#include "Slicer.h"
#include "SliceGenerator.h"

using namespace std;
using namespace llvm;

int main(int argc, char *argv[]) {
    string inputFile(argv[1]);
    Module *module;
    SMDiagnostic err;

    LLVMContext &context = getGlobalContext();
    module = ParseIRFile(inputFile, err, context);
    if (!module) {
        return 1;
    }

    ReachabilityAnalysis *ra = new ReachabilityAnalysis(module);
    ra->analyze(); 

    AAPass *aa = new AAPass();
    aa->setPAType(PointerAnalysis::AndersenWaveDiff_WPA);

    legacy::PassManager pm;
    pm.add(aa);
    pm.run(*module);

    ModRefAnalysis *mra = new ModRefAnalysis(module, ra, aa);
    mra->run();

    Annotator *annotator = new Annotator(module, mra);
    annotator->annotate();

    Cloner *cloner = new Cloner(module, mra);
    cloner->clone("f");

    SliceGenerator *sg = new SliceGenerator(module, aa, mra, cloner);
    sg->generate();
    sg->dumpSlices();

    return 0;
}
