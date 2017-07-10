#ifndef REACHABILITYANALYSIS_H
#define REACHABILITYANALYSIS_H

#include <stdio.h>
#include <vector>
#include <set>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

class ReachabilityAnalysis {
public:

    typedef std::set<llvm::Function *> FunctionSet;
    typedef std::map<llvm::Function *, FunctionSet> ReachabilityMap;

    ReachabilityAnalysis(
		llvm::Module *module,
		std::string entry,
		std::vector<std::string> targets
	) :
		module(module),
		entry(entry),
		targets(targets)
	{

    }

    ~ReachabilityAnalysis();

    bool run();

    void computeReachableFunctions(llvm::Function *entry, FunctionSet &results);

    FunctionSet &getReachableFunctions(llvm::Function *f);

    void getCallTargets(llvm::CallInst *call_inst, FunctionSet &targets);

    void dumpReachableFunctions();

private:

    void computeFunctionTypeMap();

    void updateReachabilityMap(llvm::Function *f);

    bool isVirtual(llvm::Function *f);

    llvm::Function *extractFunction(llvm::ConstantExpr *ce);

    bool removeUnreachableFunctions();

    llvm::Module *module;
    std::string entry;
    std::vector<std::string> targets;
    llvm::Function *entryFunction;
    std::vector<llvm::Function *> targetFunctions;
    std::map<llvm::FunctionType *, FunctionSet> functionTypeMap;
    ReachabilityMap reachabilityMap;
};

#endif /* REACHABILITYANALYSIS_H */
