#ifndef SVFPOINTERANALYSIS_H
#define SVFPOINTERANALYSIS_H

#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>

#include "llvm/analysis/PointsTo/PointerSubgraph.h"
#include "llvm/analysis/PointsTo/PointsTo.h"

#include "AAPass.h"

using namespace dg;
//using namespace dg::analysis::pta;

class SVFPointerAnalysis
{
public:
    SVFPointerAnalysis(llvm::Module *module, LLVMPointerAnalysis *pta, AAPass *aa) : 
        module(module),
        pta(pta),
        aa(aa) 
    {

    }

    ~SVFPointerAnalysis() {
        //std::vector<PSNode *> nodes = ps->getNodes();
        //for (PSNode *n : nodes) {
        //    MemoryObject *mo = n->getData<MemoryObject>();
        //    delete mo;
        //}
    }

    void run();
    void handleVirtualCalls();
    void handleLoad(PSNode *node);
    void handleStore(PSNode *node);
    void handleGep(PSNode *node);
    void handleCast(PSNode *node);
    void handleFuncPtr(PSNode *node);
    bool functionPointerCall(PSNode *callsite, PSNode *called);
    void handlePhi(PSNode *node);
    void handleOperand(PSNode *operand);

    PSNode *getAllocNode(ObjPN *node);
    uint64_t getAllocNodeOffset(GepObjPN *node);

    //void visit(llvm::Function &f);
    //void handleInstruction(llvm::Instruction *inst, PSNode *node);
    //void handleLoad(llvm::LoadInst *inst);
    //void handleStore(llvm::StoreInst *inst);
    //void handleCall(llvm::StoreInst *inst, PSNode *node);
    //void handleOperand(llvm::Value *addr);

    llvm::Module *module;
    LLVMPointerAnalysis *pta;
    AAPass *aa;
    std::set<PSNode *> visited;
};

#endif
