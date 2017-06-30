#ifndef INLINER_H
#define INLINER_H

#include <stdio.h>
#include <vector>

#include <llvm/IR/Module.h>

#include "ReachabilityAnalysis.h"

class Inliner {
public:

    Inliner(llvm::Module *module, ReachabilityAnalysis *ra, std::vector<std::string> targets, std::vector<std::string> functions) :
        module(module),
        ra(ra),
        targets(targets),
        functions(functions)
    {

    }

    ~Inliner();

    void run();

private:
    
    void inlineCalls(llvm::Function *f, std::vector<std::string> functions);

    llvm::Module *module;
    ReachabilityAnalysis *ra;
    std::vector<std::string> targets;
    std::vector<std::string> functions;
};

#endif /* INLINER_H */
