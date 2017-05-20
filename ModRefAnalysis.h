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

    typedef std::set<llvm::Instruction *> InstructionSet;

    typedef std::map<llvm::Function *, PointsTo> ModPtsMap;
    typedef std::map<llvm::Function *, InstructionSet> SideEffectsMap;

    typedef std::map<NodeID, InstructionSet> ObjToStoreMap;
    typedef std::map<NodeID, InstructionSet> ObjToLoadMap;
    typedef std::map<llvm::Instruction *, InstructionSet> LoadToStoreMap;

    typedef std::pair<const llvm::Value *, uint64_t> AllocSite;
    typedef std::pair<llvm::Function *, AllocSite> ModInfo;

    typedef std::map<llvm::Instruction *, std::set<ModInfo> > LoadToModInfoMap;
    typedef std::map<ModInfo, InstructionSet> ModInfoToStoreMap;
    typedef std::map<ModInfo, uint32_t> ModInfoToIdMap;
    typedef std::map<uint32_t, ModInfo> IdToModInfoMap;
    typedef std::map<llvm::Function *, uint32_t> RetSliceIdMap;

    ModRefAnalysis(
        llvm::Module *module,
        ReachabilityAnalysis *ra,
        AAPass *aa,
        std::string entry,
        std::vector<std::string> targets
    );

    void run();

    /* TODO: implement... */
    void getRetSliceId(llvm::Function *f);

    void getApproximateAllocSite(llvm::Instruction *inst, AllocSite hint);

    void dumpSideEffects();

    void dumpLoadToStoreMap();

    void dumpAllocSiteToStoreMap();

    void dumpAllocSiteToIdMap();

private:

    /* priate methods */

    void computeMod(llvm::Function *entry, llvm::Function *f);

    void collectModInfo(llvm::Function *f);

    void addStore(llvm::Function *f, llvm::Instruction *store_inst);

    void collectRefInfo(llvm::Function *entry);

    void addLoad(llvm::Instruction *load_inst);

    void computeModRefInfo();

    void computeModInfoToStoreMap();

    AllocSite getAllocSite(NodeID);

    bool hasReturnValue(llvm::Function *f);

    llvm::AliasAnalysis::Location getLoadLocation(llvm::LoadInst *inst);

    llvm::AliasAnalysis::Location getStoreLocation(llvm::StoreInst *inst);

    void dumpSideEffects(llvm::Function *f, InstructionSet &sideEffects);

    /* private members */

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    /* TODO: rename to pa (pointer analysis) */
    AAPass *aa;

    llvm::Function *entryFunction;
    std::vector<llvm::Function *> targetFunctions;

    ModPtsMap modPtsMap;
    ObjToStoreMap objToStoreMap;
    PointsTo refPts;
    ObjToLoadMap objToLoadMap;
    /* TODO: no need to hold the store instructions */
    LoadToStoreMap loadToStoreMap;
    LoadToModInfoMap loadToModInfoMap;
    /* TODO: is it required? */
    SideEffectsMap sideEffectsMap;

    //AllocSiteToStoreMap allocSiteToStoreMap;
    ModInfoToStoreMap modInfoToStoreMap;

    ModInfoToIdMap modInfoToIdMap;
    IdToModInfoMap idToModInfoMap;
    RetSliceIdMap retSliceIdMap;
    //uint32_t retSliceId;
    //SliceIds sliceIds;
};

#endif
