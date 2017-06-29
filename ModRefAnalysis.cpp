#include <stdio.h>
#include <iostream>

#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Analysis/AliasAnalysis.h>

#include "MemoryModel/PointerAnalysis.h"
#include "MSSA/MemRegion.h"
#include "MSSA/MemPartition.h"

#include "AAPass.h"
#include "ModRefAnalysis.h"

using namespace std;
using namespace llvm;

/* ModRefAnalysis class */

ModRefAnalysis::ModRefAnalysis(
    llvm::Module *module,
    ReachabilityAnalysis *ra,
    AAPass *aa,
    std::string entry,
    std::vector<std::string> targets
) :
    module(module), ra(ra), aa(aa), entry(entry), targets(targets)
{

}

Function *ModRefAnalysis::getEntry() {
    return entryFunction;
}

vector<Function *> ModRefAnalysis::getTargets() {
    return targetFunctions;
}

void ModRefAnalysis::run() {
    /* validation */
    entryFunction = module->getFunction(entry);
    if (!entryFunction) {
        errs() << "entry function '" << entry << "' is not found (or unreachable)\n";
        assert(false);
    }
    for (vector<string>::iterator i = targets.begin(); i != targets.end(); i++) {
        string name = *i;
        Function *f = module->getFunction(name);
        if (!f) {
            /* TODO: just ignore this? (warning) */
            errs() << "function '" << name << "' is not found (or unreachable)\n";
            assert(false);
        }
        targetFunctions.push_back(f);
    }

    /* collect mod information for each target function */
    for (vector<Function *>::iterator i = targetFunctions.begin(); i != targetFunctions.end(); i++) {
        Function *f = *i;
        collectModInfo(f);
    }

    /* collect ref information for the whole program (can be optimized) */
    collectRefInfo(entryFunction);

    /* compute the side effects of each target function */
    computeModRefInfo();

    /* ... */
    computeModInfoToStoreMap();

    /* debug */
    dumpModSetMap();
    dumpLoadToStoreMap();
    dumpLoadToModInfoMap();
    dumpModInfoToStoreMap();
    dumpModInfoToIdMap();
}

ModRefAnalysis::ModInfoToStoreMap &ModRefAnalysis::getModInfoToStoreMap() {
    return modInfoToStoreMap;
}

bool ModRefAnalysis::mayBlock(Instruction *load) {
    LoadToStoreMap::iterator i = loadToStoreMap.find(load);
    return i != loadToStoreMap.end();
}

ModRefAnalysis::SideEffects &ModRefAnalysis::getSideEffects() {
    return sideEffects;
}

ModRefAnalysis::ModInfoToIdMap &ModRefAnalysis::getModInfoToIdMap() {
    return modInfoToIdMap;
}

bool ModRefAnalysis::getRetSliceId(llvm::Function *f, uint32_t &id) {
    RetSliceIdMap::iterator i = retSliceIdMap.find(f);
    if (i == retSliceIdMap.end()) {
        return false;
    }

    id = i->second;
    return true;
}

void ModRefAnalysis::collectModInfo(Function *f) {
    std::stack<Function *> stack;
    std::set<Function *> pushed;
    std::set<Function *> visited;

    stack.push(f);
    pushed.insert(f);

    while (!stack.empty()) {
        Function *curr = stack.top();
        stack.pop();
        visited.insert(curr);

        for (inst_iterator iter = inst_begin(curr); iter != inst_end(curr); iter++) {
            Instruction *inst = &*iter;
            if (inst->getOpcode() == Instruction::Call) {
                CallInst *callInst = dyn_cast<CallInst>(inst);
                std::set<Function *> targets;
                ra->getCallTargets(callInst, targets);
                for (std::set<Function *>::iterator i = targets.begin(); i != targets.end(); i++) {
                    Function *target = *i;
                    if (target->isDeclaration()) {
                        continue;
                    }

                    if (pushed.find(target) == pushed.end()) {
                        stack.push(target);
                        pushed.insert(target);
                    }
                }
            }
            if (inst->getOpcode() == Instruction::Store) {
                addStore(f, inst);
            } 
        }
    }
}

void ModRefAnalysis::addStore(Function *f, Instruction *store) {
    AliasAnalysis::Location storeLocation = getStoreLocation(dyn_cast<StoreInst>(store));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(storeLocation.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    PointsTo &modPts = modPtsMap[f];
    modPts |= pts;

    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID nodeId = *i;
        pair<Function *, NodeID> k = make_pair(f, nodeId);
        objToStoreMap[k].insert(store);
    }
}

/* TODO: find a better name... */
void ModRefAnalysis::collectRefInfo(Function *entry) {
    std::stack<BasicBlock *> stack;
    std::set<BasicBlock *> visited;
    bool ignore = false;
    
    BasicBlock *initial_bb = entry->begin();
    stack.push(initial_bb);

    while (!stack.empty()) {
        BasicBlock *bb = stack.top();
        stack.pop();

        /* look for references */
        for (BasicBlock::iterator i = bb->begin(); i != bb->end(); i++) {
            Instruction *inst = &(*i);
            /* process only the instructions after the call */
            if (!ignore) {
                if (inst->getOpcode() == Instruction::Call) {
                    CallInst *callInst = dyn_cast<CallInst>(inst);
                    std::set<Function *> targets;
                    ra->getCallTargets(callInst, targets);
                    for (std::set<Function *>::iterator i = targets.begin(); i != targets.end(); i++) {
                        Function *target = *i;
                        if (target->isDeclaration()) {
                            continue;
                        }

                        BasicBlock *targetBB = target->begin();
                        if (visited.find(targetBB) == visited.end()) {
                            stack.push(targetBB);
                        }
                    }
                }
                if (inst->getOpcode() == Instruction::Load) {
                    addLoad(inst);
                }
                if (inst->getOpcode() == Instruction::Store) {
                    addOverridingStore(inst);
                }
            }
        }

        for (unsigned int i = 0; i < bb->getTerminator()->getNumSuccessors(); i++) {
            BasicBlock *successor = bb->getTerminator()->getSuccessor(i);
            if (visited.find(successor) == visited.end()) {
                stack.push(successor);
            }
        }
        
        /* mark as visited */
        visited.insert(bb);
    }
}

void ModRefAnalysis::addLoad(Instruction *load) {
    AliasAnalysis::Location loadLocation = getLoadLocation(dyn_cast<LoadInst>(load));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(loadLocation.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    refPts |= pts;
    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID nodeId = *i;
        objToLoadMap[nodeId].insert(load);
    }
}

void ModRefAnalysis::addOverridingStore(Instruction *store) {
    AliasAnalysis::Location storeLocation = getStoreLocation(dyn_cast<StoreInst>(store));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(storeLocation.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID nodeId = *i;
        objToOverridingStoreMap[nodeId].insert(store);
    }
}

void ModRefAnalysis::computeModRefInfo() {
    for (ModPtsMap::iterator i = modPtsMap.begin(); i != modPtsMap.end(); i++) {
        Function *f = i->first;
        PointsTo &modPts = i->second;

        PointsTo pts = modPts & refPts;
        InstructionSet &modSet = modSetMap[f];

        for (PointsTo::iterator ni = pts.begin(); ni != pts.end(); ++ni) {
            NodeID nodeId = *ni;

            /* update modifies-set */
            pair<Function *, NodeID> k = make_pair(f, nodeId);
            InstructionSet &stores = objToStoreMap[k];
            modSet.insert(stores.begin(), stores.end());

            InstructionSet &loads = objToLoadMap[nodeId];
            AllocSite allocSite = getAllocSite(nodeId);

            for (InstructionSet::iterator i = loads.begin(); i != loads.end(); i++) {
                Instruction *load = *i;

                /* update with store instructions */
                loadToStoreMap[load].insert(stores.begin(), stores.end());

                /* update with allocation site */
                ModInfo modInfo = make_pair(f, allocSite);
                loadToModInfoMap[load].insert(modInfo);
            }

            /* ... */
            InstructionSet &localOverridingStores = objToOverridingStoreMap[nodeId];
            overridingStores.insert(localOverridingStores.begin(), localOverridingStores.end());
        }
    }
}

void ModRefAnalysis::computeModInfoToStoreMap() {
    uint32_t sliceId = 1;

    for (vector<Function *>::iterator i = targetFunctions.begin(); i != targetFunctions.end(); i++) {
        Function *f = *i;
        InstructionSet &modSet = modSetMap[f];

        uint32_t retSliceId = sliceId++;
        if (hasReturnValue(f)) {
            retSliceIdMap[f] = retSliceId;
            SideEffect sideEffect = {
                .type = ReturnValue,
                .id = retSliceId,
                .info = {
                    .f = f
                }
            };
            sideEffects.push_back(sideEffect);
        }

        for (InstructionSet::iterator i = modSet.begin(); i != modSet.end(); i++) {
            Instruction *store = *i;
            AliasAnalysis::Location storeLocation = getStoreLocation(dyn_cast<StoreInst>(store));
            NodeID id = aa->getPTA()->getPAG()->getValueNode(storeLocation.Ptr);
            PointsTo &pts = aa->getPTA()->getPts(id);

            for (PointsTo::iterator ni = pts.begin(); ni != pts.end(); ++ni) {
                NodeID nodeId = *ni;

                /* update store instructions */
                AllocSite allocSite = getAllocSite(nodeId);
                ModInfo modInfo = make_pair(f, allocSite);
                modInfoToStoreMap[modInfo].insert(store);

                if (modInfoToIdMap.find(modInfo) == modInfoToIdMap.end()) {
                    uint32_t modSliceId = sliceId++;
                    modInfoToIdMap[modInfo] = modSliceId;
                    SideEffect sideEffect = {
                        .type = Modifier,
                        .id = modSliceId,
                        .info = {
                            .modInfo = modInfo
                        }
                    };
                    sideEffects.push_back(sideEffect);
                }
            }
        }
    }
}

ModRefAnalysis::AllocSite ModRefAnalysis::getAllocSite(NodeID nodeId) {
    PAGNode *pagNode = aa->getPTA()->getPAG()->getPAGNode(nodeId);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    assert(obj);

    /* get allocation site value */
    const MemObj *mo = obj->getMemObj();
    const Value *allocSite = mo->getRefVal();

    /* get offset in bytes */
    uint64_t offset = 0;
    if (obj->getNodeKind() == PAGNode::GepObjNode) {
        GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
        offset = gepObj->getLocationSet().getAccOffset(); 
    }

    return std::make_pair(allocSite, offset);
}

bool ModRefAnalysis::hasReturnValue(Function *f) {
    return !f->getReturnType()->isVoidTy();
}

AliasAnalysis::Location ModRefAnalysis::getLoadLocation(LoadInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

AliasAnalysis::Location ModRefAnalysis::getStoreLocation(StoreInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

/* TODO: validate that a load can't have two ModInfo's with the same allocation site */
void ModRefAnalysis::getApproximateModInfos(Instruction *inst, AllocSite hint, std::set<ModInfo> &result) {
    assert(inst->getOpcode() == Instruction::Load);

    LoadToModInfoMap::iterator entry = loadToModInfoMap.find(inst);
    if (entry == loadToModInfoMap.end()) {
        /* TODO: this should not happen */
        assert(false);
    }

    std::set<ModInfo> &modifiers = entry->second;

    for (std::set<ModInfo>::iterator i = modifiers.begin(); i != modifiers.end(); i++) {
        ModInfo modInfo = *i;
        AllocSite allocSite = modInfo.second;

        /* compare only the allocation sites (values) */
        if (allocSite.first == hint.first) {
            result.insert(modInfo);
        }
    }

    return;
}

void ModRefAnalysis::dumpModSetMap() {
    outs() << "### ModSetMap ###\n";

    for (ModSetMap::iterator i = modSetMap.begin(); i != modSetMap.end(); i++) {
        Function *f = i->first;
        InstructionSet &modSet = i->second;

        for (InstructionSet::iterator j = modSet.begin(); j != modSet.end(); j++) {
            Instruction *inst = *j;
            dumpInst(inst);
        }
    }
    outs() << "\n";
}

void ModRefAnalysis::dumpLoadToStoreMap() {
    outs() << "### LoadToStoreMap ###\n";

    for (LoadToStoreMap::iterator i = loadToStoreMap.begin(); i != loadToStoreMap.end(); i++) {
        Instruction *load = i->first;
        InstructionSet &stores = i->second;

        dumpInst(load);
        for (InstructionSet::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store = *j;

            dumpInst(store, "\t");
        }
    }
    outs() << "\n";
}

void ModRefAnalysis::dumpLoadToModInfoMap() {
    outs() << "### LoadToModInfoMap ###\n";

    for (LoadToModInfoMap::iterator i = loadToModInfoMap.begin(); i != loadToModInfoMap.end(); i++) {
        Instruction *load = i->first;
        set<ModInfo> &modInfos = i->second;

        dumpInst(load);
        for (set<ModInfo>::iterator j = modInfos.begin(); j != modInfos.end(); j++) {
            const ModInfo &modInfo = *j;
            dumpModInfo(modInfo, "\t");
        }
    }
    outs() << "\n";
}

void ModRefAnalysis::dumpModInfoToStoreMap() {
    outs() << "### ModInfoToStoreMap ###\n";

    for (ModInfoToStoreMap::iterator i = modInfoToStoreMap.begin(); i != modInfoToStoreMap.end(); i++) {
        const ModInfo &modInfo = i->first;
        InstructionSet &stores = i->second;

        dumpModInfo(modInfo);
        for (InstructionSet::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store = *j;
            dumpInst(store, "\t");
        }
    }
    outs() << "\n";
}

void ModRefAnalysis::dumpModInfoToIdMap() {
    outs() << "### ModInfoToIdMap ###\n";

    for (ModInfoToIdMap::iterator i = modInfoToIdMap.begin(); i != modInfoToIdMap.end(); i++) {
        const ModInfo &modInfo = i->first;
        uint32_t id = i->second;
        
        dumpModInfo(modInfo);
        outs() << "id: " << id << "\n";
    }
    outs() << "\n";
}

void ModRefAnalysis::dumpOverridingStores() {
    outs() << "### Overriding Stores ###\n";

    for (InstructionSet::iterator j = overridingStores.begin(); j != overridingStores.end(); j++) {
        Instruction *inst = *j;
        dumpInst(inst);
    }
}

void ModRefAnalysis::dumpInst(Instruction *inst, const char *prefix) {
    Function *f = inst->getParent()->getParent();

    outs() << prefix << "[" << f->getName() << "]";
    inst->print(outs()); 
    outs() << "\n";
}

void ModRefAnalysis::dumpModInfo(const ModInfo &modInfo, const char *prefix) {
    Function *f = modInfo.first;
    AllocSite allocSite = modInfo.second;

    const Value *value = allocSite.first;
    uint64_t offset = allocSite.second;

    outs() << prefix << "function: " << f->getName() << "\n";
    outs() << prefix << "allocation site: "; value->print(outs()); outs() << "\n";
    outs() << prefix << "offset: " << offset << "\n";
}
