#include <stdio.h>
#include <iostream>
#include <stack>
#include <set>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>

#include "ReachabilityAnalysis.h"

using namespace std;
using namespace llvm;

void ReachabilityAnalysis::run() {
    /* collect function type map for indirect calls */
    computeFunctionTypeMap();
    computeReachableFunctions(getEntryPoint(), reachable);
    removeUnreachableFunctions();
    dumpReachableFunctions();
}

void ReachabilityAnalysis::computeReachableFunctions(Function *entry, std::set<Function *> &results) {
    std::stack<Function *> stack;
    std::set<Function *> pushed;

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
            std::set<Function *> targets;
            getCallTargets(call_inst, targets);

            for (std::set<Function *>::iterator i = targets.begin(); i != targets.end(); i++) {
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

Function *ReachabilityAnalysis::getEntryPoint() {
    return module->getFunction(StringRef("main"));
}

void ReachabilityAnalysis::getCallTargets(CallInst *call_inst, std::set<Function *> &targets) {
    Function *called_function = call_inst->getCalledFunction();
    if (called_function == NULL) {
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
            std::set<Function *> functions = functionTypeMap.find(type)->second;
            targets.insert(functions.begin(), functions.end());
            outs() << functions.size() << " target functions" << "\n";
        }
    } else {
        targets.insert(called_function);
    }
}

void ReachabilityAnalysis::removeUnreachableFunctions() {
    std::set<Function *> unreachable;

    for (Module::iterator i = module->begin(); i != module->end(); i++) {
        Function *f = &*i;
        if (reachable.find(f) == reachable.end()) {
            unreachable.insert(f);
        }
    }

    for (std::set<Function *>::iterator i = unreachable.begin(); i != unreachable.end(); i++) {
        Function *f = *i;
        f->replaceAllUsesWith(UndefValue::get(f->getType()));
        f->eraseFromParent();
    }
}

void ReachabilityAnalysis::dumpReachableFunctions() {
    outs() << "### " << reachable.size() << " reachable functions ###\n";
    for (std::set<Function *>::iterator i = reachable.begin(); i != reachable.end(); i++) {
        Function *f = *i;
        outs() << "    " << f->getName() << "\n";
    }
    outs() << "\n";
}
