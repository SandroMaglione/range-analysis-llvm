#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

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
            std::vector<int> intPlaceholder;

            std::vector<ICmpInst::Predicate> listCmp;
            std::vector<Value *> listCmpOpe0;
            std::vector<Value *> listCmpOpe1;

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

                // Mark entry basic block as visited
                listRange.insert(std::pair<BasicBlock *, std::vector<int>>(BB, intPlaceholder));

                errs() << "BB: " << BB->getName() << "\n";

                // Run over all instructions in the basic block
                for (BasicBlock::InstListType::iterator it =
                         BB->getInstList().begin();
                     it != BB->getInstList().end(); ++it)
                {
                    // Get instruction from iterator
                    Instruction *I = &*it;
                    errs() << I << "\n";

                    if (auto *operInst = dyn_cast<BinaryOperator>(I))
                    {
                        errs() << "Oper\n";
                    }
                    else if (auto *brInst = dyn_cast<BranchInst>(I))
                    {
                        // If no 'if', 'while', 'for' (only one br basic block)
                        if (brInst->isUnconditional())
                        {
                            errs() << "Br-Simple\n";

                            // Check if br basic block has been already visited
                            BasicBlock *next = brInst->getSuccessor(0);
                            std::map<BasicBlock *, std::vector<int>>::iterator it = listRange.find(next);
                            // If not already visited, add to workList
                            if (it == listRange.end())
                            {
                                workList.push_back(next);
                                errs() << "New BB in workList\n";
                            }
                        }
                        else
                        {
                            errs() << "Br-Complex\n";

                            // Get previous cmp values
                            ICmpInst::Predicate pred = listCmp.at(0);
                            Value *oper0 = listCmpOpe0.at(0);
                            Value *oper1 = listCmpOpe1.at(0);
                            listCmp.pop_back();
                            listCmpOpe0.pop_back();
                            listCmpOpe1.pop_back();

                            if (pred == ICmpInst::ICMP_SLT)
                            {
                                errs() << "SLT"
                                       << "\n";
                            }
                            else if (pred == ICmpInst::ICMP_SLE)
                            {
                                errs() << "SLE"
                                       << "\n";
                            }

                            // Check successor0 if already visited
                            BasicBlock *next0 = brInst->getSuccessor(0);
                            std::map<BasicBlock *, std::vector<int>>::iterator it = listRange.find(next0);
                            // If not already visited, add to workList
                            if (it == listRange.end())
                            {
                                workList.push_back(next0);
                                errs() << "New BB in workList\n";
                            }

                            // Check successor1 if already visited
                            BasicBlock *next1 = brInst->getSuccessor(1);
                            it = listRange.find(next1);
                            // If not already visited, add to workList
                            if (it == listRange.end())
                            {
                                workList.push_back(next1);
                                errs() << "New BB in workList\n";
                            }
                        }
                    }
                    else if (auto *cmpInst = dyn_cast<CmpInst>(I))
                    {
                        errs() << "Cmp\n";
                        ICmpInst::Predicate pred = cmpInst->getPredicate();
                        listCmp.push_back(pred);
                        listCmpOpe0.push_back(cmpInst->getOperand(0));
                        listCmpOpe1.push_back(cmpInst->getOperand(1));
                    }
                    else if (auto *phiInst = dyn_cast<PHINode>(I))
                    {
                        errs() << "Phi\n";
                    }

                    errs() << "\n";
                }

                // Mark basic block as visited
                listRange.insert(std::pair<BasicBlock *, std::vector<int>>(BB, intPlaceholder));
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