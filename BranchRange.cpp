#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace
{
    struct HppsBranchRange : public FunctionPass
    {
        static char ID;
        HppsBranchRange() : FunctionPass(ID) {}

        // Run over a single function
        bool runOnFunction(Function &Func) override
        {
            std::vector<BasicBlock *> workList;
            std::map<BasicBlock *, std::vector<int>> listRange;

            // Entry block to workList
            workList.push_back(&Func.getEntryBlock());

            // Run over all basic blocks in the function
            while (workList.size() != 0)
            {
                // Get next BasicBlock in workList and remove it
                BasicBlock *BB = workList.at(0);
                workList.erase(workList.begin());

                errs() << "BB: " << BB->getName() << "\n";

                // Run over all instructions in the basic block
                for (BasicBlock::InstListType::reverse_iterator it =
                         BB->getInstList().rbegin();
                     it != BB->getInstList().rend(); it++)
                {
                    // Get instruction from iterator
                    Instruction *p = &*it;
                    errs() << p << "\n";
                }
            }

            return false;
        }
    }; // end of struct HppsBranchRange
} // end of anonymous namespace

char HppsBranchRange::ID = 0;
static RegisterPass<HppsBranchRange> X("branch-range", "Branch Range Pass",
                                       false /* Only looks at CFG */,
                                       false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new HppsBranchRange()); });