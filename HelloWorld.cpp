#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace
{
struct Hello : public FunctionPass
{
    static char ID;
    Hello() : FunctionPass(ID) {}

    // Run over a single function
    bool runOnFunction(Function &Func) override
    {
        // Run over all basic blocks in the function
        for (BasicBlock &BB : Func)
        {
            // Print out the name of the basic block if it has one, and then the
            // number of instructions that it contains
            errs() << "Basic block (name=" << BB.getName() << ") has "
                   << BB.size() << " instructions.\n";

            // Run over all instructions in the basic block
            for (Instruction &I : BB)
            {
                // The next statement works since operator<<(ostream&,...)
                // is overloaded for Instruction&
                errs() << I << "\n";
            }
        }

        errs() << "HelloWorld: ";
        errs().write_escaped(Func.getName()) << '\n';
        return false;
    }
}; // end of struct Hello
} // end of anonymous namespace

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new Hello()); });