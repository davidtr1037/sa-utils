#ifndef SLICER_H
#define SLICER_H

#include <stdio.h>

#include <llvm/IR/Module.h>

#include "AAPass.h"

int slicer_main(llvm::Module *M, AAPass *aa);

#endif /* SLICER_H */
