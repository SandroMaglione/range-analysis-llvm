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

namespace
{
  struct Hpps : public FunctionPass
  {
    static char ID;
    Hpps() : FunctionPass(ID) {}

    // Run over a single function
    bool runOnFunction(Function &Func) override
    {
      // Reference to unroll
      std::vector<Value *> from; // Value that depend on
      std::vector<Value *> to; // Value that depend to

      // Found range
      std::vector<Value *> ranged; // Reference to the value found
      std::vector<int> minRange;
      std::vector<int> maxRange;

      // Run over all basic blocks in the function
      for (BasicBlock &BB : Func)
      {
        // Run over all instructions in the basic block
        for (Instruction &I : BB)
        {
          // The next statement works since operator<<(ostream&,...)
          // is overloaded for Instruction&
          errs() << I << "\n";

          // if (auto *operInst = dyn_cast<BinaryOperator>(&I))
          // {
          //   errs() << "Operand" << operInst->getName() << "\n";
          // }

          // if (auto *loadInst = dyn_cast<LoadInst>(&I))
          // {
          //   errs() << "Load" << loadInst->getPointerOperand()->getName() << "\n";
          // }

          // STORE INSTRUCTION (Check all store in first cycle)
          if (auto *strInst = dyn_cast<StoreInst>(&I))
          { 
            // Get operand0 (value) and operand1 (assigned)
            Value *oper0 = strInst->getOperand(0);
            Value *oper1 = strInst->getOperand(1);

            // Oper0 is reference
            if (oper0->hasName())
            {
              errs() << "   -Ref:" << oper0->getName() << "\n";
              from.push_back(oper1);
              to.push_back(oper0);
            }

            // Oper0 is constant
            else
            {
              if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
              {
                errs() << "   -Const:" << CI->getZExtValue() << "\n";
                ranged.push_back(oper1);
                minRange.push_back(CI->getZExtValue());
                maxRange.push_back(CI->getZExtValue());
              }
            }

            // for (User *U : oper1->users())
            // {
            //   if (Instruction *Inst = dyn_cast<Instruction>(U))
            //   {
            //     errs() << "-USED:" << *Inst << "\n";

            //     for (Use &U : Inst->operands())
            //     {
            //       Value *v = U.get();
            //       errs() << "-DEF:" << v->getName() << "\n";
            //     }
            //   }
            // }
          }

          errs() << "\n";
        }
      }

      // Print references
      errs() << "\nREFERENCES:\n";
      for (auto i = 0; i < from.size(); ++i)
      {
        errs() << to.at(i)->getName() << "(" << to.at(i) << ") <-" << from.at(i)->getName() << "\n";
      }

      // Print value range computed
      errs() << "\nVALUE RANGES:\n";
      for (auto i = 0; i < minRange.size(); ++i)
      {
        errs() << ranged.at(i)->getName() << "(" << minRange.at(i) << ", " << maxRange.at(i) << ")\n";
      }

      return false;
    }
  }; // namespace
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