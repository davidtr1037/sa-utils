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
    typedef std::pair<const llvm::Value *, uint64_t> AllocSite;
    typedef std::map<NodeID, std::set<llvm::Instruction *> > ObjToStoreMap;
    typedef std::map<NodeID, std::set<llvm::Instruction *> > ObjToLoadMap;
    typedef std::map<llvm::Instruction *, std::set<llvm::Instruction *> > LoadToStoreMap;
    typedef std::map<llvm::Instruction *, std::set<AllocSite> > LoadToAllocSiteMap;
    typedef std::map<AllocSite, std::set<llvm::Instruction *> > AllocSiteToStoreMap;
    typedef std::map<AllocSite, uint32_t> AllocSiteToIdMap;
    typedef std::vector<uint32_t> SliceIds;

    ModRefAnalysis(llvm::Module *module, ReachabilityAnalysis *ra, AAPass *aa, std::string entry, std::string target) :
        module(module), ra(ra), aa(aa), entry(entry), target(target)
    {
    
    }

    void run();

    void computeMod(llvm::Function *entry, llvm::Function *f);

    llvm::Instruction *findInitialInst(llvm::Function *entry, llvm::Function *f);

    void collectModInfo(llvm::Function *f);

    void addStore(llvm::Instruction *store_inst);

    void collectRefInfo(llvm::Instruction *initial_inst);

    void addLoad(llvm::Instruction *load_inst);

    void getModRefInfo();

    void computeAllocSiteToStoreMap();

    void computeAllocSiteToIdMap();

    AllocSite getApproximateAllocSite(llvm::Instruction *inst, AllocSite hint);

    void dumpMod();

    void dumpLoadToStoreMap();

    void dumpAllocSiteToStoreMap();

    void dumpAllocSiteToIdMap();

private:
    AllocSite getAllocSite(NodeID);

    bool hasReturnValue();

    llvm::AliasAnalysis::Location getLoadLocation(llvm::LoadInst *inst);

    llvm::AliasAnalysis::Location getStoreLocation(llvm::StoreInst *inst);

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    AAPass *aa;

public:
    std::string entry;
    std::string target;

    PointsTo modPts;
    ObjToStoreMap objToStoreMap;
    PointsTo refPts;
    ObjToLoadMap objToLoadMap;
    /* TODO: check if required... */
    LoadToStoreMap loadToStoreMap;
    LoadToAllocSiteMap loadToAllocSiteMap;
    std::set<llvm::Instruction *> sideEffects;
    AllocSiteToStoreMap allocSiteToStoreMap;

    AllocSiteToIdMap allocSiteToIdMap;
    uint32_t retSliceId;
    SliceIds sliceIds;
};

#endif
