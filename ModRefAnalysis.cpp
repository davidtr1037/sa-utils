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

void ModRefAnalysis::run() {
    Function *entryFunction = module->getFunction(entry);
    assert(entryFunction);
    Function *targetFunction = module->getFunction(target);
    assert(targetFunction);

    computeMod(entryFunction, targetFunction);

    dumpMod();
    dumpLoadToStoreMap();
    dumpAllocSiteToStoreMap();
}

void ModRefAnalysis::computeMod(Function *entry, Function *f) {
    /* initialize side effects */
    sideEffects.clear();

    //Instruction *initial_inst = findInitialInst(entry, f);
    //if (initial_inst == NULL) {
    //    return;
    //}

    collectModInfo(f);
    collectRefInfo(entry->begin()->begin());
    getModRefInfo();
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
                addStore(inst);
            } 
        }
    }
}

void ModRefAnalysis::addStore(Instruction *store_inst) {
    AliasAnalysis::Location store_location = getStoreLocation(dyn_cast<StoreInst>(store_inst));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(store_location.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    modPts |= pts;
    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID node_id = *i;
        objToStoreMap[node_id].insert(store_inst);
    }
}

//Instruction *ModRefAnalysis::findInitialInst(Function *entry, Function *f) {
//    std::stack<BasicBlock *> stack;
//    std::set<BasicBlock *> visited;
//
//    stack.push(entry->begin());
//
//    while (!stack.empty()) {
//        BasicBlock *bb = stack.top();
//        stack.pop();
//
//        visited.insert(bb);
//
//        /* iterate over instructions */
//        for (BasicBlock::iterator i = bb->begin(); i != bb->end(); i++) {
//            Instruction *inst = &(*i);
//            if (inst->getOpcode() == Instruction::Call) {
//                CallInst *call_inst = dyn_cast<CallInst>(inst);
//                Function *called_function = call_inst->getCalledFunction();
//                if (called_function == f) {
//                    return inst;
//                }
//            }
//        }
//
//        for (unsigned int i = 0; i < bb->getTerminator()->getNumSuccessors(); i++) {
//            BasicBlock *successor = bb->getTerminator()->getSuccessor(i);
//            if (visited.find(successor) == visited.end()) {
//                stack.push(successor);
//            }
//        }
//    }
//
//    return NULL;
//}

void ModRefAnalysis::collectRefInfo(Instruction *initial_inst) {
    std::stack<BasicBlock *> stack;
    std::set<BasicBlock *> visited;
    bool ignore = false;
    
    BasicBlock *initial_bb = initial_inst->getParent();
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

void ModRefAnalysis::getModRefInfo() {
    PointsTo pts = modPts & refPts;
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
            loadToAllocSiteMap[load_inst].insert(allocSite);
        }
    }

    computeAllocSiteToStoreMap();
    computeAllocSiteToIdMap();
}

void ModRefAnalysis::computeAllocSiteToStoreMap() {
    for (std::set<Instruction *>::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        Instruction *inst = *i; 
        AliasAnalysis::Location storeLocation = getStoreLocation(dyn_cast<StoreInst>(inst));
        NodeID id = aa->getPTA()->getPAG()->getValueNode(storeLocation.Ptr);
        PointsTo &pts = aa->getPTA()->getPts(id);

        for (PointsTo::iterator ni = pts.begin(); ni != pts.end(); ++ni) {
            NodeID nodeId = *ni;

            /* update store instructions */
            AllocSite allocSite = getAllocSite(nodeId);
            allocSiteToStoreMap[allocSite].insert(inst);  
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

void ModRefAnalysis::computeAllocSiteToIdMap() {
    uint32_t id = 1;

    /* TODO: better solution? */
    retSliceId = id++;
    if (hasReturnValue()) {
        sliceIds.push_back(retSliceId);
    }

    for (AllocSiteToStoreMap::iterator i = allocSiteToStoreMap.begin(); i != allocSiteToStoreMap.end(); i++) {
        allocSiteToIdMap[i->first] = id;
        sliceIds.push_back(id);
        id++;
    }
}

bool ModRefAnalysis::hasReturnValue() {
    Function *targetFunction = module->getFunction(target);
    return !targetFunction->getReturnType()->isVoidTy();
}

AliasAnalysis::Location ModRefAnalysis::getLoadLocation(LoadInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

AliasAnalysis::Location ModRefAnalysis::getStoreLocation(StoreInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

ModRefAnalysis::AllocSite ModRefAnalysis::getApproximateAllocSite(Instruction *inst, AllocSite hint) {
    assert(inst->getOpcode() == Instruction::Load);

    LoadToAllocSiteMap::iterator entry = loadToAllocSiteMap.find(inst);
    if (entry == loadToAllocSiteMap.end()) {
        /* this should not happen */
        assert(false);
    }

    std::set<AllocSite> &allocSites = entry->second;
    std::set<AllocSite> approximateAllocSites;
    for (std::set<AllocSite>::iterator i = allocSites.begin(); i != allocSites.end(); i++) {
        AllocSite allocSite = *i;
        if (allocSite == hint) {
            return allocSite;
        }

        if (allocSite.first == hint.first) {
            approximateAllocSites.insert(allocSite);
        }
    }

    /* TODO: this assumption does not hold if we have a buffer in a struct */
    assert(approximateAllocSites.size() == 1);
    return *approximateAllocSites.begin();
}

void ModRefAnalysis::dumpMod() {
    outs() << "side effects (" << sideEffects.size() << ")\n";
    for (std::set<Instruction *>::iterator i = sideEffects.begin(); i != sideEffects.end(); i++) {
        Instruction *inst = *i;
        outs() <<  "[" << inst->getParent()->getParent()->getName() << "]";
        inst->print(outs());
        outs() << "\n";
    }
}

void ModRefAnalysis::dumpLoadToStoreMap() {
    outs() << "LoadToStoreMap:\n";
    for (LoadToStoreMap::iterator i = loadToStoreMap.begin(); i != loadToStoreMap.end(); i++) {
        Instruction *load = i->first;
        set<Instruction *> stores = i->second;
        outs() << "LOAD: "; load->print(outs()); outs() << "\n";
        for (set<Instruction *>::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store_inst = *j;
            outs() << "-- STORE: "; store_inst->print(outs()); outs() << "\n";
        }
    }
}

void ModRefAnalysis::dumpAllocSiteToStoreMap() {
    outs() << "AllocSiteToStoreMap:\n";
    for (AllocSiteToStoreMap::iterator i = allocSiteToStoreMap.begin(); i != allocSiteToStoreMap.end(); i++) {
        std::pair<const Value *, uint64_t> key = i->first;
        set<Instruction *> stores = i->second;
        const Value *allocSite = key.first;
        uint64_t offset = key.second;
        outs() << "AS: "; allocSite->print(outs()); outs() << "\n";
        outs() << "OFFSET: " << offset << "\n";

        for (set<Instruction *>::iterator j = stores.begin(); j != stores.end(); j++) {
            Instruction *store = *j;
            outs() << "-- STORE: "; store->print(outs()); outs() << "\n";
        }
    }
}

void ModRefAnalysis::dumpAllocSiteToIdMap() {
    errs() << "AllocSiteToIdMap:\n";
    for (AllocSiteToIdMap::iterator i = allocSiteToIdMap.begin(); i != allocSiteToIdMap.end(); i++) {
        std::pair<const Value *, uint64_t> key = i->first;
        const Value *allocSite = key.first;
        uint64_t offset = key.second;
        errs() << "AS: "; allocSite->print(errs()); errs() << "\n";
        errs() << "OFFSET: " << offset << "\n";
        errs() << "ID: " << i->second << "\n";
    }
}

/* Temporary */

Instruction *get_inst(Function *f, unsigned int k) {
    inst_iterator iter = inst_begin(f);
    for (unsigned int i = 0; i < k; i++) {
        iter++;
    }
    return &*iter;
}
