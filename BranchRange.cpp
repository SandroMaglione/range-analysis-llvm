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

            // Max int (infinity)
            int infMax = std::numeric_limits<int>::max();
            int infMin = std::numeric_limits<int>::min();

            // Create Null range reference
            std::pair<int, int> emptyIntPair(infMin, infMax);
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

                // Get vector from 'listRange' of current basic block (to update with new ranges)
                std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator iterBB = listRange.find(BB);
                std::vector<std::pair<Value *, std::pair<int, int>>> listRangeBB = iterBB->second;

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
                            updateWorkList(brInst->getSuccessor(0), &listRange, &workList);
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

                            // Default new range pair and value
                            std::pair<int, int> intRangePair(infMin, infMax);
                            Value *oper = oper0;

                            if (oper0->hasName())
                            {
                                // Select reference to operand0
                                oper = oper0;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    // TODO: We compute the range based on the Predicate (already done).
                                    // Then we need to see if the Value associated with the range is already present in the
                                    // 'listRange' for the current basic block.
                                    // If it is, then see if the range change, if it changes COMBINE it
                                    // If it is not, then add it as it is
                                    // If it is, and it does not change, then nothing has to be updated

                                    if (pred == ICmpInst::ICMP_SLT)
                                    {
                                        errs() << oper->getName() << " < " << CI->getZExtValue() << "\n";
                                        intRangePair.second = CI->getZExtValue() - 1;
                                    }
                                    else if (pred == ICmpInst::ICMP_SGT)
                                    {
                                        errs() << oper->getName() << " > " << CI->getZExtValue() << "\n";
                                        intRangePair.first = CI->getZExtValue() + 1;
                                    }
                                }
                            }
                            else
                            {
                                // Select reference to operand0
                                oper = oper1;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                                {
                                    if (pred == ICmpInst::ICMP_SLT)
                                    {
                                        errs() << oper->getName() << " > " << CI->getZExtValue() << "\n";
                                        intRangePair.first = CI->getZExtValue() + 1;
                                    }
                                    else if (pred == ICmpInst::ICMP_SGT)
                                    {
                                        errs() << oper->getName() << " < " << CI->getZExtValue() << "\n";
                                        intRangePair.second = CI->getZExtValue() - 1;
                                    }
                                }
                            }

                            // Check for new range and add it in case of updates
                            std::pair<Value *, std::pair<int, int>> rangePair(oper, intRangePair);
                            computeRange(BB, oper, &listRange, listRangeBB, intRangePair, rangePair);

                            // Check successor0 if already visited
                            updateWorkList(brInst->getSuccessor(0), &listRange, &workList);
                            updateWorkList(brInst->getSuccessor(1), &listRange, &workList);
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

            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator resIt;
            for (resIt = listRange.begin(); resIt != listRange.end(); ++resIt)
            {
                errs() << "BB: " << resIt->first->getName() << "\n";
                std::vector<std::pair<Value *, std::pair<int, int>>> listBB = resIt->second;
                for (unsigned v = 0; v < listBB.size(); ++v)
                {
                    std::pair<Value *, std::pair<int, int>> pairBB = listBB.at(v);

                    errs() << "   " << pairBB.first->getName() << "(" << pairBB.second.first << ", " << pairBB.second.second << ")\n";
                }
                errs() << "\n";
            }

            return false;
        }

        void computeRange(BasicBlock *BB, Value *oper, std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>> *listRange, std::vector<std::pair<Value *, std::pair<int, int>>> listRangeBB, std::pair<int, int> intRangePair, std::pair<Value *, std::pair<int, int>> rangePair)
        {
            bool isAlreadyRef = false;

            for (unsigned i = 0; i < listRangeBB.size(); ++i)
            {
                Value *val = listRangeBB.at(i).first;
                std::pair<int, int> valRange = listRangeBB.at(i).second;

                errs() << val->getName();

                if (val == oper && (valRange.first != intRangePair.first || valRange.second != intRangePair.second))
                {
                    // Already in the list but range has changed, COMBINE the ranges
                    errs() << "ALREADY THERE!\n";
                    std::pair<int, int> newPairRange(std::min(valRange.first, intRangePair.first), std::max(valRange.second, intRangePair.second));
                    listRange->find(BB)->second.erase(listRange->find(BB)->second.begin() + i);
                    listRange->find(BB)->second.push_back(std::pair<Value *, std::pair<int, int>>(oper, newPairRange));
                    isAlreadyRef = true;
                    break;
                }
            }

            // Not already in the list, add it
            if (!isAlreadyRef || listRangeBB.size() == 0)
            {
                errs() << "NEW ADDED!\n";
                listRange->find(BB)->second.push_back(rangePair);
            }
        }

        void updateWorkList(BasicBlock *next, std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>> *listRange, std::vector<BasicBlock *> *workList)
        {
            // Check successor1 if already visited
            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator it = listRange->find(next);

            // If not already visited, add to workList
            if (it == listRange->end())
            {
                workList->push_back(next);
                errs() << "New BB in workList\n";
            }
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

// bool isAlreadyRef = false;

// for (unsigned i = 0; i < listRangeBB.size(); ++i)
// {
//     Value *val = listRangeBB.at(i).first;
//     std::pair<int, int> valRange = listRangeBB.at(i).second;

//     errs() << val->getName();

//     if (val == oper0 && (valRange.first != intRangePair.first || valRange.second != intRangePair.second))
//     {
//         // Already in the list but range has changed, COMBINE the ranges
//         errs() << "ALREADY THERE!\n";
//         std::pair<int, int> newPairRange(std::min(valRange.first, intRangePair.first), std::max(valRange.second, intRangePair.second));
//         listRange.find(BB)->second.erase(listRange.find(BB)->second.begin() + i);
//         listRange.find(BB)->second.push_back(std::pair<Value *, std::pair<int, int>>(oper0, newPairRange));
//         isAlreadyRef = true;
//         break;
//     }
// }

// // Not already in the list, add it
// if (!isAlreadyRef || listRangeBB.size() == 0)
// {
//     errs() << "NEW ADDED!\n";
//     listRange.find(BB)->second.push_back(rangePair);
// }