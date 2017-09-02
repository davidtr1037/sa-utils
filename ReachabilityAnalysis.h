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

#include "AAPass.h"

class ReachabilityAnalysis {
public:

    typedef std::set<llvm::Function *> FunctionSet;
    typedef std::map<llvm::Function *, FunctionSet> ReachabilityMap;
    typedef std::map<llvm::FunctionType *, FunctionSet> FunctionTypeMap;

    ReachabilityAnalysis(
        llvm::Module *module,
        std::string entry,
        std::vector<std::string> targets,
        llvm::raw_ostream &debugs
    ) :
        module(module),
        entry(entry),
        targets(targets),
        aa(NULL),
        debugs(debugs)
    {

    }

    ~ReachabilityAnalysis() {};

    /* must be called before making any reachability analysis */
    void prepare();

    void usePA(AAPass *aa) {
        this->aa = aa;
    }

    bool run(bool usePA);

    void computeReachableFunctions(
        llvm::Function *entry,
        bool usePA,
        FunctionSet &results
    );

    FunctionSet &getReachableFunctions(llvm::Function *f);

    void dumpReachableFunctions();

private:

    void removeUnusedValues();

    bool removeUnusedValues(bool &changed);

    void computeFunctionTypeMap();

    void updateReachabilityMap(llvm::Function *f, bool usePA);

    bool isVirtual(llvm::Function *f);

    void getCallTargets(
        llvm::CallInst *callInst,
        bool usePA,
        FunctionSet &targets
    );

    void resolveIndirectCallByType(llvm::Type *calledType, FunctionSet &targets);

    void resolveIndirectCallByPA(llvm::Value *calledValue, FunctionSet &targets);

    llvm::Function *extractFunction(llvm::ConstantExpr *ce);

    llvm::Module *module;
    std::string entry;
    std::vector<std::string> targets;
    llvm::Function *entryFunction;
    std::vector<llvm::Function *> targetFunctions;
    FunctionTypeMap functionTypeMap;
    ReachabilityMap reachabilityMap;
    AAPass *aa;
    llvm::raw_ostream &debugs;
};

#endif /* REACHABILITYANALYSIS_H */
