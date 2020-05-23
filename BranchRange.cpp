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
            // Create a Null Value reference
            // Assigned as placeholder when no variable reference available
            LLVMContext &context = Func.getContext();
            Value *nullValue = ConstantInt::get(Type::getInt32Ty(context), -1);

            // Create Null range reference
            std::pair<int, int> emptyIntPair(0, 0);
            std::pair<Value *, std::pair<int, int>> emptyPair(nullValue, emptyIntPair);
            std::vector<std::pair<Value *, std::pair<int, int>>> emptyVector;

            // Save cmp instructions to resolve on br instructions
            std::vector<ICmpInst::Predicate> listCmp;
            std::vector<Value *> listCmpOpe0;
            std::vector<Value *> listCmpOpe1;

            // List of basic blocks left to cycle
            std::vector<BasicBlock *> workList;

            // For each basic block, store list of ranges
            // Contains list of value reference and current min and max range for that value
            // {
            //      "BB1": [ { '%k', { 0, 100 } } ]
            // }
            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>> listRange;

            // Entry block to workList
            workList.push_back(&Func.getEntryBlock());

            // Loop on the worklist until all dependencies are resolved
            while (workList.size() != 0)
            {
                // Get next BasicBlock in workList and remove it
                BasicBlock *BB = workList.at(0);
                workList.erase(workList.begin());

                // Mark entry basic block as visited
                listRange.insert(std::pair<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>(BB, emptyVector));

                // Get index of 'listRange' of current basic block
                std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator iterBB = listRange.find(BB);
                
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
                            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator it = listRange.find(next);
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

                            if (oper0->hasName())
                            {
                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    if (pred == ICmpInst::ICMP_SLT)
                                    {
                                        errs() << oper0->getName() << " < " << CI->getZExtValue() << "\n";
                                        std::pair<int, int> intRangePair(0, CI->getZExtValue() - 1);
                                        std::pair<Value *, std::pair<int, int>> rangePair(oper0, intRangePair);
                                    }
                                    else if (pred == ICmpInst::ICMP_SGT)
                                    {
                                        errs() << oper0->getName() << " > " << CI->getZExtValue() << "\n";
                                        std::pair<int, int> intRangePair(CI->getZExtValue() + 1, 0);
                                        std::pair<Value *, std::pair<int, int>> rangePair(oper0, intRangePair);
                                    }
                                }
                            }
                            else
                            {
                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                                {
                                    if (pred == ICmpInst::ICMP_SLT)
                                    {
                                        errs() << oper1->getName() << " > " << CI->getZExtValue() << "\n";
                                        std::pair<int, int> intRangePair(CI->getZExtValue() + 1, 0);
                                        std::pair<Value *, std::pair<int, int>> rangePair(oper1, intRangePair);
                                    }
                                    else if (pred == ICmpInst::ICMP_SGT)
                                    {
                                        errs() << oper1->getName() << " < " << CI->getZExtValue() << "\n";
                                        std::pair<int, int> intRangePair(0, CI->getZExtValue() - 1);
                                        std::pair<Value *, std::pair<int, int>> rangePair(oper1, intRangePair);
                                    }
                                }
                            }

                            // Check successor0 if already visited
                            BasicBlock *next0 = brInst->getSuccessor(0);
                            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator it = listRange.find(next0);
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
                listRange.insert(std::pair<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>(BB, emptyVector));
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