#include <stdio.h>
#include <iostream>
#include <vector>
#include <stack>
#include <set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

#include "AAPass.h"
#include "ReachabilityAnalysis.h"

using namespace std;
using namespace llvm;

void ReachabilityAnalysis::prepare() {
    /* remove unused functions using fixpoint */
    removeUnusedValues();
    /* compute function type map for resolving indirect calls */
    computeFunctionTypeMap();
}

void ReachabilityAnalysis::removeUnusedValues() {
    bool changed;

    do {
        removeUnusedValues(changed);
    } while (changed);
}

bool ReachabilityAnalysis::removeUnusedValues(bool &changed) {
    std::set<Function *> functions;
    set<string> keep;

    keep.insert(entry);

    for (Module::iterator i = module->begin(); i != module->end(); i++) {
        Function *f = &*i;
        if (keep.find(f->getName().str()) != keep.end()) {
            continue;
        }

        if (f->hasNUses(0)) {
            functions.insert(f);
        }
    }

    for (Function *f : functions) {
        debugs << "erasing: " << f->getName() << "\n";
        f->eraseFromParent();
    }

    changed = !functions.empty();
}

void ReachabilityAnalysis::computeFunctionTypeMap() {
    for (Module::iterator i = module->begin(); i != module->end(); i++) {
        /* add functions which may be virtual */
        Function *f = &*i;
        if (!isVirtual(f)) {
            continue;
        }

        FunctionType *type = f->getFunctionType();
        functionTypeMap[type].insert(f);

        /* if a function pointer is casted, consider the casted type as well */
        for (auto i = f->use_begin(); i != f->use_end(); i++) {
            ConstantExpr *ce = dyn_cast<ConstantExpr>(*i);
            if (ce && ce->isCast()) {
                PointerType *pointerType = dyn_cast<PointerType>(ce->getType());
                if (!pointerType) {
                    continue;
                }

                FunctionType *castedType = dyn_cast<FunctionType>(pointerType->getElementType());
                if (!castedType) {
                    continue;
                }

                functionTypeMap[castedType].insert(f);
            }
        }
    }
}

bool ReachabilityAnalysis::run(bool usePA) {
    vector<Function *> all;

    /* check parameters... */
    entryFunction = module->getFunction(entry);
    if (!entryFunction) {
        errs() << "entry function '" << entry << "' is not found\n";
        return false;
    }
    all.push_back(entryFunction);

    for (vector<string>::iterator i = targets.begin(); i != targets.end(); i++) {
        string name = *i;
        Function *f = module->getFunction(name);
        if (!f) {
            errs() << "function '" << name << "' is not found\n";
            return false;
        }
        targetFunctions.push_back(f);
        all.push_back(f);
    }

    /* build reachability map */
    for (vector<Function *>::iterator i = all.begin(); i != all.end(); i++) {
        updateReachabilityMap(*i, usePA);
    }

    /* debug */
    dumpReachableFunctions();

    return true;
}

void ReachabilityAnalysis::updateReachabilityMap(Function *f, bool usePA) {
    FunctionSet &functions = reachabilityMap[f];
    computeReachableFunctions(f, usePA, functions);
}

void ReachabilityAnalysis::computeReachableFunctions(
    Function *entry,
    bool usePA,
    FunctionSet &results
) {
    stack<Function *> stack;
    FunctionSet pushed;

    stack.push(entry);
    pushed.insert(entry);
    results.insert(entry);

    while (!stack.empty()) {
        Function *f = stack.top();
        stack.pop();

        for (inst_iterator iter = inst_begin(f); iter != inst_end(f); iter++) {
            Instruction *inst = &*iter;
            if (inst->getOpcode() != Instruction::Call) {
                continue;
            }

            CallInst *callInst = dyn_cast<CallInst>(inst);

            /* potential call targets */
            FunctionSet targets;
            getCallTargets(callInst, usePA, targets);

            for (FunctionSet::iterator i = targets.begin(); i != targets.end(); i++) {
                Function *target = *i;
                results.insert(target);

                if (target->isDeclaration()) {
                    continue;
                }

                if (pushed.find(target) == pushed.end()) {
                    stack.push(target);
                    pushed.insert(target);
                }
            }
        }
    }
}

bool ReachabilityAnalysis::isVirtual(Function *f) {
    for (Value::use_iterator i = f->use_begin(); i != f->use_end(); i++) {
        Value *use = *i;
        CallInst *callInst = dyn_cast<CallInst>(use);
        if (!callInst) {
            /* we found a use which is not a call instruction */
            return true;
        }

        for (unsigned int j = 0; j < callInst->getNumArgOperands(); j++) {
            Value *arg = callInst->getArgOperand(j);
            if (isa<Function>(arg)) {
                if (arg == f) {
                    /* the function is passed as an argument */
                    return true;
                }
            }
        }
    }

    return false;
}

void ReachabilityAnalysis::getCallTargets(
    CallInst *callInst,
    bool usePA,
    FunctionSet &targets
) {
    Function *calledFunction = callInst->getCalledFunction();
    Value *calledValue = callInst->getCalledValue();

    if (!calledFunction) {
        /* the called value should be one of these: function pointer, cast, alias */
        if (isa<ConstantExpr>(calledValue)) {
            Function *extracted = extractFunction(dyn_cast<ConstantExpr>(calledValue));
            if (!extracted) {
                /* TODO: unexpected value... */
                assert(false);
            }
            calledFunction = extracted;
        }
    }

    if (calledFunction == NULL) {
        /* the called value should be a function pointer */
        Type *calledType = callInst->getOperand(callInst->getNumOperands() - 1)->getType();
        Type *elementType = dyn_cast<PointerType>(calledType)->getElementType();
        if (!elementType) {
            return;
        }

        FunctionType *type = dyn_cast<FunctionType>(elementType);
        if (!type) {
            return;
        }

        FunctionTypeMap::iterator i = functionTypeMap.find(type);
        if (i != functionTypeMap.end()) {
            FunctionSet &functions = i->second;
            targets.insert(functions.begin(), functions.end());
        }
    } else {
        targets.insert(calledFunction);
    }
}

Function *ReachabilityAnalysis::extractFunction(ConstantExpr *ce) {
    if (!ce->isCast()) {
        return NULL;
    }

    Value *value = ce->getOperand(0);
    if (isa<Function>(value)) {
        return dyn_cast<Function>(value);
    }

    if (isa<GlobalAlias>(value)) {
        Constant *aliasee = dyn_cast<GlobalAlias>(value)->getAliasee();
        if (isa<Function>(aliasee)) {
            return dyn_cast<Function>(aliasee);
        }
        if (isa<ConstantExpr>(aliasee)) {
            return extractFunction(dyn_cast<ConstantExpr>(aliasee));
        }
    }

    return NULL;
}

ReachabilityAnalysis::FunctionSet &ReachabilityAnalysis::getReachableFunctions(Function *f) {
    ReachabilityMap::iterator i = reachabilityMap.find(f);
    if (i == reachabilityMap.end()) {
        assert(false);
    }

    return i->second;
}

void ReachabilityAnalysis::dumpReachableFunctions() {
    /* get all reachable functions */
    FunctionSet &reachable = getReachableFunctions(entryFunction);

    debugs << "### " << reachable.size() << " reachable functions ###\n";
    for (FunctionSet::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        debugs << "    " << f->getName() << "\n";
    }
    debugs << "\n";
}
