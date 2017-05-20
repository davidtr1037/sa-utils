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
    module(module), ra(ra), aa(aa)
{
    entryFunction = module->getFunction(entry);
    assert(entryFunction);
    for (vector<string>::iterator i = targets.begin(); i != targets.end(); i++) {
        Function *f = module->getFunction(*i);
        assert(f);
        targetFunctions.push_back(f);
    }
}


void ModRefAnalysis::run() {
    /* collect mod information for each target function */
    for (vector<Function *>::iterator i = targetFunctions.begin(); i != targetFunctions.end(); i++) {
        Function *f = *i;
        collectModInfo(f);
    }

    /* collect ref information for the whole program (can be optimized) */
    collectRefInfo(entryFunction);
    /* ... */
    computeModRefInfo();
    /* ... */
    computeModInfoToStoreMap();

    dumpSideEffects();
    dumpLoadToStoreMap();
    dumpAllocSiteToIdMap();
    dumpAllocSiteToStoreMap();
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
                CallInst *call_inst = dyn_cast<CallInst>(inst);
                std::set<Function *> targets;
                ra->getCallTargets(call_inst, targets); 
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

void ModRefAnalysis::addStore(Function *f, Instruction *store_inst) {
    AliasAnalysis::Location store_location = getStoreLocation(dyn_cast<StoreInst>(store_inst));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(store_location.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    PointsTo &modPts = modPtsMap[f];
    modPts |= pts;

    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID node_id = *i;
        objToStoreMap[node_id].insert(store_inst);
    }
}

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
                    CallInst *call_inst = dyn_cast<CallInst>(inst);
                    std::set<Function *> targets;
                    ra->getCallTargets(call_inst, targets); 
                    for (std::set<Function *>::iterator i = targets.begin(); i != targets.end(); i++) {
                        Function *target = *i;
                        if (target->isDeclaration()) {
                            continue;
                        }

                        BasicBlock *target_bb = target->begin();
                        if (visited.find(target_bb) == visited.end()) {
                            stack.push(target_bb);
                        }
                    }
                }
                /* TODO: is it enough to check LOAD instructions? */
                if (inst->getOpcode() == Instruction::Load) {
                    addLoad(inst);
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

void ModRefAnalysis::addLoad(Instruction *load_inst) {
    AliasAnalysis::Location load_location = getLoadLocation(dyn_cast<LoadInst>(load_inst));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(load_location.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    refPts |= pts;
    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID node_id = *i;
        objToLoadMap[node_id].insert(load_inst);
    }
}

void ModRefAnalysis::computeModRefInfo() {
    for (ModPtsMap::iterator i = modPtsMap.begin(); i != modPtsMap.end(); i++) {
        Function *f = i->first;
        PointsTo &modPts = i->second;

        PointsTo pts = modPts & refPts;
        InstructionSet &sideEffects = sideEffectsMap[f];

        for (PointsTo::iterator ni = pts.begin(); ni != pts.end(); ++ni) {
            NodeID node_id = *ni;
            set<Instruction *> stores = objToStoreMap[node_id];
            sideEffects.insert(stores.begin(), stores.end());

            set<Instruction *> load_insts = objToLoadMap[node_id];
            for (set<Instruction *>::iterator i = load_insts.begin(); i != load_insts.end(); i++) {
                Instruction *load_inst = *i;

                /* update with store instructions */
                loadToStoreMap[load_inst].insert(stores.begin(), stores.end());

                /* update with allocation site */
                AllocSite allocSite = getAllocSite(node_id);
                ModInfo modInfo = make_pair(f, allocSite);
                loadToModInfoMap[load_inst].insert(modInfo);
            }
        }
    }
}

void ModRefAnalysis::computeModInfoToStoreMap() {
    for (vector<Function *>::iterator i = targetFunctions.begin(); i != targetFunctions.end(); i++) {
        Function *f = *i;
        InstructionSet &sideEffects = sideEffectsMap[f];
        uint32_t id = 0;

        uint32_t retSliceId = id++;
        if (hasReturnValue(f)) {
            retSliceIdMap[f] = retSliceId;
        }

        for (InstructionSet::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
            Instruction *inst = *i;
            AliasAnalysis::Location storeLocation = getStoreLocation(dyn_cast<StoreInst>(inst));
            NodeID id = aa->getPTA()->getPAG()->getValueNode(storeLocation.Ptr);
            PointsTo &pts = aa->getPTA()->getPts(id);

            for (PointsTo::iterator ni = pts.begin(); ni != pts.end(); ++ni) {
                NodeID nodeId = *ni;

                /* update store instructions */
                AllocSite allocSite = getAllocSite(nodeId);
                ModInfo modInfo = make_pair(f, allocSite);
                modInfoToStoreMap[modInfo].insert(inst);

                if (modInfoToIdMap.find(modInfo) == modInfoToIdMap.end()) {
                    uint32_t sliceId = id++;
                    modInfoToIdMap[modInfo] = sliceId;
                    idToModInfoMap[sliceId] = modInfo;
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

void ModRefAnalysis::getApproximateAllocSite(Instruction *inst, AllocSite hint) {
    assert(inst->getOpcode() == Instruction::Load);

    LoadToModInfoMap::iterator entry = loadToModInfoMap.find(inst);
    if (entry == loadToModInfoMap.end()) {
        /* this should not happen */
        assert(false);
    }

    std::set<ModInfo> &modifiers = entry->second;
    std::set<ModInfo> approximateModifiers;

    /* TODO: choose the most precise ModInfo for each function */
    for (std::set<ModInfo>::iterator i = modifiers.begin(); i != modifiers.end(); i++) {
        ModInfo modInfo = *i;
        AllocSite allocSite = modInfo.second;
        if (allocSite == hint) {
            return;
        }

        if (allocSite.first == hint.first) {
            approximateModifiers.insert(modInfo);
        }
    }

    if (approximateModifiers.empty()) {
        /* TODO: something went wrong with the static analysis... */
        assert(false);
    }

    /* TODO: this assumption does not hold if we have a buffer in a struct */
    if (approximateModifiers.size() > 1) {
        assert(false);
    }

    return;
}

void ModRefAnalysis::dumpSideEffects() {
    outs() << "side effects\n";
    for (SideEffectsMap::iterator i = sideEffectsMap.begin(); i != sideEffectsMap.end(); i++) {
        Function *f = i->first;
        InstructionSet &sideEffects = i->second;
        dumpSideEffects(f, sideEffects);
    }
}

void ModRefAnalysis::dumpSideEffects(Function *f, InstructionSet &sideEffects) {
    for (std::set<Instruction *>::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        Instruction *inst = *i;
        outs() <<  "[" << f->getName() << "]";
        inst->print(outs());
        outs() << "\n";
    }
}

void ModRefAnalysis::dumpLoadToStoreMap() {
    outs() << "LoadToStoreMap:\n";
    for (LoadToStoreMap::iterator i = loadToStoreMap.begin(); i != loadToStoreMap.end(); i++) {
        Instruction *load = i->first;
        set<Instruction *> stores = i->second;

        outs() << "LOAD: " << "[" << load->getParent()->getParent()->getName() << "]";
        load->print(outs()); outs() << "\n";

        for (set<Instruction *>::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store_inst = *j;
            outs() << "-- STORE: " << "[" << store_inst->getParent()->getParent()->getName() << "]";
            store_inst->print(outs()); outs() << "\n";
        }
    }
}

void ModRefAnalysis::dumpAllocSiteToStoreMap() {
    outs() << "ModInfoToStoreMap:\n";
    for (ModInfoToStoreMap::iterator i = modInfoToStoreMap.begin(); i != modInfoToStoreMap.end(); i++) {
        ModInfo modInfo = i->first;
        InstructionSet &stores = i->second;

        Function *f = modInfo.first;
        AllocSite allocSite = modInfo.second;

        const Value *value = allocSite.first;
        uint64_t offset = allocSite.second;

        outs() << "function: " << f->getName() << "\n";
        outs() << "allocation site: "; value->print(outs()); outs() << "\n";
        outs() << "offset: " << offset << "\n";

        for (InstructionSet::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store = *j;
            outs() << "-- store: "; store->print(outs()); outs() << "\n";
        }
    }
}

void ModRefAnalysis::dumpAllocSiteToIdMap() {
    outs() << "ModInfoToIdMap:\n";
    for (ModInfoToIdMap::iterator i = modInfoToIdMap.begin(); i != modInfoToIdMap.end(); i++) {
        ModInfo modInfo = i->first;
        uint32_t id = i->second;

        Function *f = modInfo.first;
        AllocSite allocSite = modInfo.second;

        const Value *value = allocSite.first;
        uint64_t offset = allocSite.second;

        outs() << "function: " << f->getName() << "\n";
        outs() << "allocation site: "; value->print(outs()); outs() << "\n";
        outs() << "offset: " << offset << "\n";
        outs() << "id: " << id << "\n";
    }
}
