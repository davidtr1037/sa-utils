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

#include "ReachabilityAnalysis.h"

using namespace std;
using namespace llvm;

bool ReachabilityAnalysis::run() {
    /* remove unused functions using fixpoint */
    removeUnusedValues();

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

    /* collect function type map for indirect calls */
    computeFunctionTypeMap();

    /* build reachability map */
    for (vector<Function *>::iterator i = all.begin(); i != all.end(); i++) {
        updateReachabilityMap(*i);
    }

    /* TODO: can cause problems in some cases... */
    //if (!removeUnreachableFunctions()) {
    //    return false;
    //}

    /* debug */
    dumpReachableFunctions();

    return true;
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
        f->eraseFromParent();
    }

    changed = !functions.empty();
}

void ReachabilityAnalysis::computeFunctionTypeMap() {
    for (Module::iterator i = module->begin(); i != module->end(); i++) {
        /* add functions which may be virtual */
        Function *f = &*i;
        if (isVirtual(f)) {
            FunctionType *type = f->getFunctionType();
            functionTypeMap[type].insert(f);
        }
    }
}

void ReachabilityAnalysis::updateReachabilityMap(Function *f) {
    FunctionSet &functions = reachabilityMap[f];
    computeReachableFunctions(f, functions);
}

void ReachabilityAnalysis::computeReachableFunctions(
    Function *entry,
    FunctionSet &results
) {
    stack<Function *> stack;
    FunctionSet pushed;

    if (!entry) {
        return;
    }

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

            CallInst *call_inst = dyn_cast<CallInst>(inst);

            /* potential call targets */
            FunctionSet targets;
            getCallTargets(call_inst, targets);

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
    for (Value::use_iterator x = f->use_begin(); x != f->use_end(); x++) {
        Value *use = *x;
        CallInst *call_inst = dyn_cast<CallInst>(use);
        if (!call_inst) {
            /* we found a use which is not a call instruction */
            return true;
        }

        for (unsigned int i = 0; i < call_inst->getNumArgOperands(); i++) {
            Function *arg = dyn_cast<Function>(call_inst->getArgOperand(i));
            if (arg && arg == f) {
                /* the function is passed as an argument */
                return true;
            }
        }
    }

    return false;
}

void ReachabilityAnalysis::getCallTargets(CallInst *call_inst, FunctionSet &targets) {
    Function *called_function = call_inst->getCalledFunction();
    Value *calledValue = call_inst->getCalledValue();

    if (!called_function) {
        /* the called value should be one of these: function pointer, cast, alias */
        if (isa<ConstantExpr>(calledValue)) {
            Function *extracted = extractFunction(dyn_cast<ConstantExpr>(calledValue));
            if (!extracted) {
                /* TODO: unexpected value... */
                assert(false);
            }
            called_function = extracted;
        }
    }

    if (called_function == NULL) {
        /* the called value should be a function pointer */
        Type *called_type = call_inst->getOperand(call_inst->getNumOperands() - 1)->getType();
        Type *elementType = dyn_cast<PointerType>(called_type)->getElementType();
        if (!elementType) {
            return;
        }

        FunctionType *type = dyn_cast<FunctionType>(elementType);
        if (!type) {
            return;
        }

        outs() << "indirect call: ";
        type->print(outs());
        outs() << "\n";

        if (functionTypeMap.find(type) != functionTypeMap.end()) {
            FunctionSet &functions = functionTypeMap.find(type)->second;
            targets.insert(functions.begin(), functions.end());
            outs() << functions.size() << " target functions" << "\n";
        }
    } else {
        targets.insert(called_function);
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

/* TODO: handle klee_* functions */
bool ReachabilityAnalysis::removeUnreachableFunctions() {
    /* get all reachable functions */
    FunctionSet &reachable = getReachableFunctions(entryFunction);

    /* find unreachable functions */
    FunctionSet unreachable;
    for (Module::iterator i = module->begin(); i != module->end(); i++) {
        Function *f = &*i;
        if (reachable.find(f) == reachable.end()) {
            if (reachabilityMap.find(f) != reachabilityMap.end()) {
                /* TODO: warn about it... */
                return false;
            }
            unreachable.insert(f);
        }
    }

    for (FunctionSet::iterator i = unreachable.begin(); i != unreachable.end(); i++) {
        Function *f = *i;
        f->replaceAllUsesWith(UndefValue::get(f->getType()));
        f->eraseFromParent();
    }

    return true;
}

void ReachabilityAnalysis::dumpReachableFunctions() {
    /* get all reachable functions */
    FunctionSet &reachable = getReachableFunctions(entryFunction);

    outs() << "### " << reachable.size() << " reachable functions ###\n";
    for (FunctionSet::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        outs() << "    " << f->getName() << "\n";
    }
    outs() << "\n";
}
