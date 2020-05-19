#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"

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
      // Keep reference of value still left to find the ranges
      // 'from' is unknown, it depends on 'to', 'toValue', and 'toOps' ('+' or '-')
      // from = to toOps toValue (a = b + 1)
      // from = toValue toOps to (a = 1 + b)
      // from = to toOps to2 (a = b + c)
      std::vector<Value *> from;
      std::vector<Value *> to;
      std::vector<int> toValue;
      std::vector<Value *> to2;
      std::vector<unsigned> toOps;

      // Reference of variabiles for which range has been found
      std::vector<Value *> ranged; // Reference to the variable
      std::vector<int> minRange;   // Min range of variable
      std::vector<int> maxRange;   // Max range of variable

      // Keep reference of variables in load instructions
      std::vector<Value *> loadRef;

      // Run over all basic blocks in the function
      for (BasicBlock &BB : Func)
      {
        // Run over all instructions in the basic block
        for (Instruction &I : BB)
        {
          // Print instruction
          errs() << I << "\n";

          // When instruction is load, store reference to its variables
          if (auto *loadInst = dyn_cast<LoadInst>(&I))
          {
            for (Use &U : loadInst->operands())
            {
              Value *v = U.get();
              loadRef.push_back(v);
              errs() << "   -Load:" << v->getName() << "\n";
            }
          }

          // When instruction is Add or Sub
          // Save operand0 (reference/constant) and operand1 (reference/constant)
          if (auto *operInst = dyn_cast<BinaryOperator>(&I))
          {
            Value *oper0 = operInst->getOperand(0);
            Value *oper1 = operInst->getOperand(1);

            // Store reference of variable still to find range
            from.push_back(operInst);
            toOps.push_back(operInst->getOpcode()); // '+' ot '-'

            // Store constants/references
            errs() << "   -Stored:" << operInst->getName() << "\n";

            // a = b + 1 (variable + constant)
            // Get reference from previous load instruction
            if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
            {
              toValue.push_back(CI->getZExtValue());
              to.push_back(loadRef.at(0));
              loadRef.pop_back();
            }

            // a = 1 + b (constant + variable)
            // Get reference from previous load instruction
            else if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
            {
              toValue.push_back(CI->getZExtValue());
              to.push_back(loadRef.at(0));
              loadRef.pop_back();
            }

            // a = b + c (variable + variable)
            // Get reference from two previous load instructions
            else
            {
              // TODO: It may be that there are x2 'to', create a struct to handle them both
              to.push_back(loadRef.at(0));
              to.push_back(loadRef.at(1));

              // Remove previous used load instruction value
              loadRef.pop_back();
              loadRef.pop_back();
            }
          }

          // When instruction is store
          if (auto *strInst = dyn_cast<StoreInst>(&I))
          {
            // Get operand0 (value) and operand1 (assigned)
            // operand1 = operand0
            Value *oper0 = strInst->getOperand(0);
            Value *oper1 = strInst->getOperand(1);

            // Store reference to variable still to find range
            // a = b
            if (oper0->hasName())
            {
              errs() << "   -Ref:" << oper0->getName() << "\n";
              from.push_back(oper1); // a
              to.push_back(oper0);   // b

              toValue.push_back(0);              // Placeholder
              toOps.push_back(Instruction::Add); // Placeholder
            }

            // Store constant range value (Value-Range Found!)
            // a = 1
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
          }

          errs() << "\n";
        }
      }

      // Resolve references
      errs() << "\nREFERENCES:\n";
      for (unsigned i = 0; i < from.size(); ++i)
      {
        errs() << to.at(i)->getName() << "+" << toValue.at(i) << "<-" << from.at(i)->getName() << "\n";

        // Search variable reference inside already found ranges
        for (unsigned v = 0; v < ranged.size(); ++v)
        {
          // When range found, add a new found range
          if (to.at(i) == ranged.at(v))
          {
            errs() << "-FOUND:" << ranged.at(v)->getName() << "\n";
            if (toOps.at(i) == Instruction::Add)
            {
              minRange.push_back(minRange.at(v) + toValue.at(i));
              maxRange.push_back(minRange.at(v) + toValue.at(i));
            }
            else if (toOps.at(i) == Instruction::Sub)
            {
              minRange.push_back(minRange.at(v) - toValue.at(i));
              maxRange.push_back(minRange.at(v) - toValue.at(i));
            }

            ranged.push_back(from.at(i));
          }
        }
      }

      // Print value range computed
      errs() << "\nVALUE RANGES\n";
      for (unsigned i = 0; i < minRange.size(); ++i)
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