#ifndef MODREFANALYSIS_H
#define MODREFANALYSIS_H

#include <stdbool.h>
#include <iostream>
#include <set>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Analysis/AliasAnalysis.h>

#include "ReachabilityAnalysis.h"
#include "AAPass.h"

class ModRefAnalysis {
public:
    typedef std::map<NodeID, std::set<llvm::Instruction *> > ObjToStoreMap;
    typedef std::map<NodeID, std::set<llvm::Instruction *> > ObjToLoadMap;
    typedef std::map<llvm::Instruction *, std::set<llvm::Instruction *> > LoadToStoreMap;
    typedef std::map<std::pair<const llvm::Value *, uint64_t>, std::set<llvm::Instruction *> > AllocSiteToStoreMap;

    ModRefAnalysis(llvm::Module *module, ReachabilityAnalysis *ra, AAPass *aa) : 
        module(module), ra(ra), aa(aa) 
    {
    
    }

    void run();

    void computeMod(llvm::Function *entry, llvm::Function *f);

    llvm::Instruction *findInitialInst(llvm::Function *entry, llvm::Function *f);

    void collectModInfo(llvm::Function *f);

    void addStore(llvm::Instruction *store_inst);

    void collectRefInfo(llvm::Instruction *initial_inst);

    void addLoad(llvm::Instruction *load_inst);

    void checkReference(llvm::Instruction *inst);

    bool isRelevant(llvm::Instruction *load_inst);

    bool isNonLocalObject(NodeID id);

    bool checkAlias(llvm::Instruction *load, llvm::Instruction *store);

    llvm::AliasAnalysis::Location getLoadLocation(llvm::LoadInst *inst);

    llvm::AliasAnalysis::Location getStoreLocation(llvm::StoreInst *inst);

    void getModRefInfo();

    void computeAllocSiteToStoreMap();

    void dumpMod();

    void dumpLoadToStoreMap();

    void dumpAllocSiteToStoreMap();

private:
    llvm::Module *module;
    ReachabilityAnalysis *ra;
    AAPass *aa;

public:
    PointsTo modPts;
    ObjToStoreMap objToStoreMap;
    PointsTo refPts;
    ObjToLoadMap objToLoadMap;
    LoadToStoreMap loadToStoreMap;
    std::set<llvm::Instruction *> sideEffects;
    AllocSiteToStoreMap allocSiteToStoreMap;
};

#endif
