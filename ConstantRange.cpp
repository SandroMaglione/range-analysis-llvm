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

    // Run over a single function (main)
    bool runOnFunction(Function &Func) override
    {
      // Create a Null Value reference
      // Assigned as placeholder when no variable reference available
      LLVMContext &context = Func.getContext();
      Value *nullValue = ConstantInt::get(Type::getInt32Ty(context), -1);

      // Keep reference of values still left to find the ranges
      // 'from' is unknown, it depends on 'to', 'to2', 'toValue', and 'toOps' ('+' or '-')
      //
      // from = to toOps toValue (a = b + 1)
      // from = toValue toOps to2 (a = 1 + b)
      // from = to toOps to2 (a = b + c)
      std::vector<Value *> from;
      std::vector<Value *> to;
      std::vector<Value *> to2;
      std::vector<int> toValue;
      std::vector<unsigned> toOps;

      // Reference of variables for which range has been found
      // Note: Since no branches, minRange = maxRange (constant values)
      std::vector<Value *> ranged; // Reference to the variable (name)
      std::vector<int> minRange;   // Min range of variable
      std::vector<int> maxRange;   // Max range of variable

      // Keep reference of variables (alias) from load instructions
      std::vector<Value *> loadRef;

      // Run over all basic blocks in the function
      for (BasicBlock &BB : Func)
      {
        // Run over all instructions in the basic block
        for (Instruction &I : BB)
        {
          // Print instruction
          errs() << I << "\n";

          // When instruction is load, store reference (alias) to its real variables
          if (auto *loadInst = dyn_cast<LoadInst>(&I))
          {
            // Cycle over all the operands and store them for later use in 'loadRef'
            // Used to identify references in binary operations
            for (Use &U : loadInst->operands())
            {
              Value *v = U.get();
              loadRef.push_back(v);
              errs() << "   -Load: " << v->getName() << "\n";
            }
          }

          // When instruction is Add or Sub (a = a + c or a = b + 1 or a = 1 + b)
          // Save operand0 (reference/constant) and operand1 (reference/constant)
          if (auto *operInst = dyn_cast<BinaryOperator>(&I))
          {
            // Get operands from binary operation
            Value *oper0 = operInst->getOperand(0);
            Value *oper1 = operInst->getOperand(1);

            // Store reference of variable still to find range
            from.push_back(operInst);               // left-hand side of the operation
            toOps.push_back(operInst->getOpcode()); // '+' ot '-'

            // Print variable assigned and name of operands
            errs() << "   -Var: " << operInst->getName() << "\n";
            errs() << "     -Op0: " << oper0->getName() << "\n";
            errs() << "     -Op1: " << oper1->getName() << "\n";

            // a = b + 1 (variable + constant)
            // Get reference from previous load instruction
            if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
            {
              toValue.push_back(CI->getZExtValue());
              to2.push_back(nullValue);

              // If operand0 is reference (and not an alias) store it
              if (oper0->hasName())
              {
                to.push_back(oper0);
              }
              // Otherwise get value from previous load instruction (alias)
              else
              {
                to.push_back(loadRef.at(0));
                loadRef.pop_back();
              }
            }

            // a = 1 + b (constant + variable)
            // Get reference from previous load instruction
            else if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
            {
              toValue.push_back(CI->getZExtValue());
              to.push_back(nullValue);

              // If operand1 is reference (and not an alias) store it
              if (oper1->hasName())
              {
                to2.push_back(oper1);
              }
              // Otherwise get value from previous load instruction (alias)
              else
              {
                to2.push_back(loadRef.at(0));
                loadRef.pop_back();
              }
            }

            // a = b + c (variable + variable)
            // Get reference from two previous load instructions
            else
            {
              toValue.push_back(0); // Placeholder

              // Both operand0 and operand1 as reference
              if (oper0->hasName() && oper1->hasName())
              {
                to.push_back(oper0);
                to2.push_back(oper1);
              }
              // Operand0 from load and operand1 as reference
              else if (!oper0->hasName() && oper1->hasName())
              {
                to.push_back(loadRef.at(0));
                to2.push_back(oper1);
                loadRef.pop_back();
              }
              // Operand1 from load and operand0 as reference
              else if (oper0->hasName() && !oper1->hasName())
              {
                to.push_back(oper0);
                to2.push_back(loadRef.at(0));
                loadRef.pop_back();
              }
              // Both operand0 and operand1 from load (alias)
              else
              {
                to.push_back(loadRef.at(0));
                to2.push_back(loadRef.at(1));
                loadRef.pop_back();
                loadRef.pop_back();
              }
            }
          }

          // When instruction is store (a = b or a = 1)
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
              errs() << "   -Ref: " << oper0->getName() << "\n";
              from.push_back(oper1); // a
              to.push_back(oper0);   // b

              to2.push_back(nullValue);          // Placeholder
              toValue.push_back(0);              // Placeholder
              toOps.push_back(Instruction::Add); // Placeholder
            }

            // Store constant range value (Value-Range Found!)
            // a = 1 (variable assigned constant value)
            else
            {
              if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
              {
                errs() << "   -Range found: " << CI->getZExtValue() << "\n";

                // Store found value-range
                ranged.push_back(oper1);
                minRange.push_back(CI->getZExtValue());
                maxRange.push_back(CI->getZExtValue());
              }
            }
          }

          errs() << "\n";
        }
      }

      // Resolve references/dependencies
      errs() << "\n--- REFERENCES ---\n";
      for (unsigned i = 0; i < from.size(); ++i)
      {
        // Print all dependencies
        errs() << from.at(i)->getName() << "(";
        if (to.at(i) == nullValue)
        {
          errs() << toValue.at(i) << ", ";
        }
        else
        {
          errs() << to.at(i)->getName() << ", ";
        }
        if (to2.at(i) == nullValue)
        {
          errs() << toValue.at(i) << ")\n";
        }
        else
        {
          errs() << to2.at(i)->getName() << ")\n";
        }

        // Get value from ranged ranges
        int refValue1 = -1;
        int refValue2 = -1;

        // If 'to' is placeholder, then value is constant number
        if (to.at(i) == nullValue)
        {
          refValue1 = toValue.at(i);
        }

        // If 'to2' is placeholder, then value is constant number
        if (to2.at(i) == nullValue)
        {
          refValue2 = toValue.at(i);
        }

        // Search variable reference inside already found ranges
        for (unsigned v = 0; v < ranged.size(); ++v)
        {
          // When found value of 'to' in 'ranged'
          if (to.at(i) == ranged.at(v))
          {
            // When range found, add a new found range
            refValue1 = minRange.at(v);
          }

          // When found value of 'to2' in 'ranged'
          if (to2.at(i) == ranged.at(v))
          {
            // When range found, add a new found range
            refValue2 = minRange.at(v);
          }
        }

        // Compute found value-range from resolved dependencies
        if (toOps.at(i) == Instruction::Add)
        {
          minRange.push_back(refValue1 + refValue2);
          maxRange.push_back(refValue1 + refValue2);
          ranged.push_back(from.at(i));
          errs() << "  " << from.at(i)->getName() << " = " << refValue1 << " + " << refValue2 << " = " << (refValue1 + refValue2) << "\n\n";
        }
        else if (toOps.at(i) == Instruction::Sub)
        {
          minRange.push_back(refValue1 - refValue2);
          maxRange.push_back(refValue1 - refValue2);
          ranged.push_back(from.at(i));
          errs() << "  " << from.at(i)->getName() << " = " << refValue1 << " - " << refValue2 << " = " << (refValue1 - refValue2) << "\n\n";
        }
      }

      // Print value range computed
      errs() << "\n--- VALUE RANGES ---\n";
      for (unsigned i = 0; i < minRange.size(); ++i)
      {
        errs() << " " << ranged.at(i)->getName() << "(" << minRange.at(i) << ", " << maxRange.at(i) << ")\n";
      }

      return false;
    }
  }; // namespace
} // end of anonymous namespace

char Hpps::ID = 0;
static RegisterPass<Hpps> X("const-range", "Constant Range Pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new Hpps());
                                });