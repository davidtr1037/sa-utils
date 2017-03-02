#ifndef REACHABILITYANALYSIS_H
#define REACHABILITYANALYSIS_H

#include <stdio.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

class ReachabilityAnalysis {
public:
    ReachabilityAnalysis(llvm::Module *module) : module(module) {

	}

    ~ReachabilityAnalysis();

	void analyze();

    void getCallTargets(llvm::CallInst *call_inst, std::set<llvm::Function *> &targets);

private:
    void computeFunctionTypeMap();

    void computeReachableFunctions();

    bool isVirtual(llvm::Function *f);

    void removeUnreachableFunctions();

    llvm::Function *getEntryPoint();

    llvm::Module *module;
    std::map<llvm::FunctionType *, std::set<llvm::Function *> > functionTypeMap;
    std::set<llvm::Function *> reachable;
};

#endif /* REACHABILITYANALYSIS_H */
