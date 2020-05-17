#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct Hpps : public FunctionPass {
  static char ID;
  Hpps() : FunctionPass(ID) {}

  // Run over a single function
  bool runOnFunction(Function &Func) override {
    std::vector<Instruction *> worklist;

    // Vector of min and max value-ranges
    std::vector<StringRef> ref;
    std::vector<int> minRange;
    std::vector<int> maxRange;

    // Run over all basic blocks in the function
    for (BasicBlock &BB : Func) {
      // Print out the name of the basic block if it has one, and then the
      // number of instructions that it contains
      errs() << "Basic block (name=" << BB.getName() << ") has " << BB.size()
             << " instructions.\n";

      // Run over all instructions in the basic block
      for (Instruction &I : BB) {
        // The next statement works since operator<<(ostream&,...)
        // is overloaded for Instruction&
        errs() << I << "\n";

        // Check if the instruction is a store
        if (auto *strInst = dyn_cast<StoreInst>(&I)) {
          // Get value of right operand (assignment)
          Value *oper = strInst->getOperand(0);
          Value *operRef = strInst->getOperand(1);
          if (operRef->hasName()) {
            StringRef stringRef = oper->getName();
            ref.push_back(stringRef);
          }

          // If does not have a name, then it is a constant
          if (oper->hasName()) {
            errs() << "   -REG: "
                   << "(" << oper->getName() << ")\n";

            minRange.push_back(-1);
            maxRange.push_back(-1);
          } else {
            errs() << "   -CONST: "
                   << "\n";
            if (ConstantInt *CI = dyn_cast<ConstantInt>(oper)) {
              errs() << "   -> Value=" << CI->getValue() << "\n";
              errs() << "   -> BitWidth=" << CI->getBitWidth() << "\n";
              minRange.push_back(CI->getZExtValue());
              maxRange.push_back(CI->getZExtValue());
            } else {
              errs() << "   -> (?)\n";
              minRange.push_back(-1);
              maxRange.push_back(-1);
            }
          }

          worklist.push_back(&I);
        }

        errs() << "\n";
      }
    }

    errs() << "\nVALUE RANGES:\n";
    for (int i = 0; i < minRange.size(); ++i) {
      errs() << ref.at(i) << "(" << minRange.at(i) << ", " << maxRange.at(i)
             << ")\n";
    }

    return false;
  }
}; // end of struct Hpps
} // end of anonymous namespace

char Hpps::ID = 0;
static RegisterPass<Hpps> X("hpps", "Hpps World Pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new Hpps());
                                });