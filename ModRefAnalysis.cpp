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
    Function *entry = module->getFunction(StringRef("main"));
    Function *f = module->getFunction(StringRef("f"));
    computeMod(entry, f);
    dumpMod();
    //dumpLoadToStoreMap();
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

bool ModRefAnalysis::isRelevant(Instruction *load_inst) {
    AliasAnalysis::Location load_location = getLoadLocation(dyn_cast<LoadInst>(load_inst));
    NodeID id = aa->getPTA()->getPAG()->getValueNode(load_location.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it !=eit; ++it) {
        if (isNonLocalObject(*it)) {
            return true;
        }
    }
    return false;
}

bool ModRefAnalysis::isNonLocalObject(NodeID id) {
    const MemObj* obj = aa->getPTA()->getPAG()->getObject(id);
    assert(obj && "object not found!!");

    if (obj->isGlobalObj() || obj->isHeap()) {
        return true;
    }

    if (obj->isStack()) {
        return false;
    }

    return false;
}

bool ModRefAnalysis::checkAlias(Instruction *load, Instruction *store) {
    AliasAnalysis::Location load_location = getLoadLocation(dyn_cast<LoadInst>(load));
    AliasAnalysis::Location store_location = getStoreLocation(dyn_cast<StoreInst>(store));

    /* check aliasing */
    AliasAnalysis::AliasResult result = aa->alias(load_location, store_location);
    if (result == AliasAnalysis::NoAlias) {
        return false;
    }
    
    NodeID id = aa->getPTA()->getPAG()->getValueNode(store_location.Ptr);
    PointsTo &pts = aa->getPTA()->getPts(id);

    return true;
}

AliasAnalysis::Location ModRefAnalysis::getLoadLocation(LoadInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

AliasAnalysis::Location ModRefAnalysis::getStoreLocation(StoreInst *inst) {
    Value *addr = inst->getPointerOperand();
    return AliasAnalysis::Location(addr);
}

void ModRefAnalysis::getModRefInfo() {
    PointsTo pts = modPts & refPts;
    for (PointsTo::iterator i = pts.begin(); i != pts.end(); ++i) {
        NodeID node_id = *i;
        set<Instruction *> store_insts = objToStoreMap[node_id];
        sideEffects.insert(store_insts.begin(), store_insts.end());

        set<Instruction *> load_insts = objToLoadMap[node_id];
        for (set<Instruction *>::iterator i = load_insts.begin(); i != load_insts.end(); i++) {
            Instruction *load_inst = *i;
            loadToStoreMap[load_inst].insert(store_insts.begin(), store_insts.end());
        }
    }
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
        set<Instruction *> store_insts = i->second;
        outs() << "LOAD: "; load->print(outs()); outs() << "\n";
        for (set<Instruction *>::iterator j = store_insts.begin(); j != store_insts.end(); j++) {
            Instruction *store_inst = *j;
            outs() << "-- STORE: "; store_inst->print(outs()); outs() << "\n";
        }
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
