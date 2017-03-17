#ifndef SLICER_H
#define SLICER_H

#include <stdio.h>

#include <llvm/IR/Module.h>

#include "llvm/LLVMDependenceGraph.h"
#include "llvm/Slicer.h"
#include "llvm/analysis/PointsTo/PointsTo.h"
#include "llvm/analysis/ReachingDefinitions/ReachingDefinitions.h"

#include "AAPass.h"

using namespace dg;
using namespace dg::analysis::rd;

class Slicer {
private:
    uint32_t slice_id = 0;
    bool got_slicing_criterion = true;

protected:
    llvm::Module *M;
    uint32_t opts = 0;
    AAPass *svfaa;
    std::string entryFunction;
    std::vector<std::string> criterions;
    std::unique_ptr<LLVMPointerAnalysis> PTA;
    std::unique_ptr<LLVMReachingDefinitions> RD;
    LLVMDependenceGraph dg;
    LLVMSlicer slicer;

public:
    Slicer(llvm::Module *mod, uint32_t o, AAPass *svfaa, std::string entryFunction, std::vector<std::string> criterions);
    int run();
    bool buildDG();
    bool mark();
    void computeEdges();
    bool slice();
    void remove_unused_from_module_rec();
    bool remove_unused_from_module();
    void make_declarations_external();
    const LLVMDependenceGraph& getDG() const { return dg; }
    LLVMDependenceGraph& getDG() { return dg; }
};

#endif /* SLICER_H */
