#ifndef MODREFANALYSIS_H
#define MODREFANALYSIS_H

#include <stdbool.h>
#include <iostream>
#include <set>
#include <map>
#include <vector>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Analysis/AliasAnalysis.h>

#include "ReachabilityAnalysis.h"
#include "AAPass.h"

class ModRefAnalysis {
public:

    typedef std::set<llvm::Instruction *> InstructionSet;

    typedef std::map<llvm::Function *, PointsTo> ModPtsMap;
    typedef std::map<llvm::Function *, InstructionSet> ModSetMap;

    typedef std::map<std::pair<llvm::Function *, NodeID>, InstructionSet> ObjToStoreMap;
    typedef std::map<NodeID, InstructionSet> ObjToLoadMap;
    typedef std::map<llvm::Instruction *, InstructionSet> LoadToStoreMap;

    typedef std::pair<const llvm::Value *, uint64_t> AllocSite;
    typedef std::pair<llvm::Function *, AllocSite> ModInfo;

    typedef std::map<llvm::Instruction *, std::set<ModInfo> > LoadToModInfoMap;
    typedef std::map<ModInfo, InstructionSet> ModInfoToStoreMap;
    typedef std::map<ModInfo, uint32_t> ModInfoToIdMap;
    typedef std::map<uint32_t, ModInfo> IdToModInfoMap;
    typedef std::map<llvm::Function *, uint32_t> RetSliceIdMap;

    typedef enum {
        Modifier,
        ReturnValue,
    } SideEffectType;

    typedef struct {
        SideEffectType type;
        /* TODO: remove this member */
        llvm::Function *f;
        uint32_t id;
        /* TODO: use a union instead */
        ModInfo modInfo;
    } SideEffect;

    typedef std::vector<SideEffect> SideEffects;

    ModRefAnalysis(
        llvm::Module *module,
        ReachabilityAnalysis *ra,
        AAPass *aa,
        std::string entry,
        std::vector<std::string> targets
    );

    llvm::Function *getEntry();

    std::vector<llvm::Function *> getTargets();

    void run();

    ModInfoToStoreMap &getModInfoToStoreMap();

    SideEffects &getSideEffects();

    ModInfoToIdMap &getModInfoToIdMap();

    bool mayBlock(llvm::Instruction *load);

    uint32_t getRetSliceId(llvm::Function *f);

    void getApproximateModInfos(llvm::Instruction *inst, AllocSite hint, std::set<ModInfo> &result);

    void dumpModSetMap();

    void dumpLoadToStoreMap();

    void dumpLoadToModInfoMap();

    void dumpModInfoToStoreMap();

    void dumpModInfoToIdMap();

    void dumpInst(llvm::Instruction *load, const char *prefix = "");
    
    void dumpModInfo(ModInfo &modInfo, const char *prefix = "");

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

    /* private members */

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    AAPass *aa;

    llvm::Function *entryFunction;
    std::vector<llvm::Function *> targetFunctions;

    ModPtsMap modPtsMap;
    ObjToStoreMap objToStoreMap;
    PointsTo refPts;
    ObjToLoadMap objToLoadMap;

    ModSetMap modSetMap;

    /* TODO: no need to hold the store instructions */
    LoadToStoreMap loadToStoreMap;

    LoadToModInfoMap loadToModInfoMap;
    ModInfoToStoreMap modInfoToStoreMap;

    ModInfoToIdMap modInfoToIdMap;
    RetSliceIdMap retSliceIdMap;

    SideEffects sideEffects;
};

#endif
