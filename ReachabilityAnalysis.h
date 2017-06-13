#ifndef REACHABILITYANALYSIS_H
#define REACHABILITYANALYSIS_H

#include <stdio.h>
#include <set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

class ReachabilityAnalysis {
public:
    ReachabilityAnalysis(llvm::Module *module) : module(module) {

    }

    ~ReachabilityAnalysis();

    void run();

    void computeReachableFunctions(llvm::Function *entry, std::set<llvm::Function *> &results);

    void getCallTargets(llvm::CallInst *call_inst, std::set<llvm::Function *> &targets);

    void dumpReachableFunctions();

private:

    void computeFunctionTypeMap();

    bool isVirtual(llvm::Function *f);

    llvm::Function *getCastedFunction(llvm::ConstantExpr *ce);

    void removeUnreachableFunctions();

    llvm::Function *getEntryPoint();

    llvm::Module *module;
    std::map<llvm::FunctionType *, std::set<llvm::Function *> > functionTypeMap;
    std::set<llvm::Function *> reachable;
};

#endif /* REACHABILITYANALYSIS_H */
