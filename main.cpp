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
#include "Slicer.h"

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

    AAPass *pass = new AAPass();
    pass->setPAType(PointerAnalysis::AndersenWaveDiff_WPA);

    legacy::PassManager pm;
    pm.add(pass);
    pm.run(*module);

    ModRefAnalysis *mra = new ModRefAnalysis(module, ra, pass);
    mra->run();

    Annotator *annotator = new Annotator(module, mra);
    annotator->annotate();

    std::vector<std::string> criterions;
    criterions.push_back("__crit_0");
    //criterions.push_back("__crit_1");
    Slicer *slicer = new Slicer(module, 0, pass, "f", criterions);
    slicer->run();

    return 0;
}
