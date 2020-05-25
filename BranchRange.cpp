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

                errs() << "--- " << BB->getName() << " ---\n";

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
                        // Get operands from binary operation
                        Value *oper0 = operInst->getOperand(0);
                        Value *oper1 = operInst->getOperand(1);

                        // Print variable assigned and name of operands
                        errs() << "   -Var: " << operInst->getName() << "\n";
                        errs() << "     -Op0: " << oper0->getName() << "\n";
                        errs() << "     -Op1: " << oper1->getName() << "\n";

                        // // a = b + 1 (variable + constant)
                        // if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                        // {
                        //     toValue.push_back(CI->getZExtValue());
                        //     to2.push_back(nullValue);

                        //     // If operand0 is reference (and not an alias) store it
                        //     if (oper0->hasName())
                        //     {
                        //         to.push_back(oper0);
                        //     }
                        //     // Otherwise get value from previous load instruction (alias)
                        //     else
                        //     {
                        //         to.push_back(loadRef.at(0));
                        //         loadRef.pop_back();
                        //     }
                        // }
                    }
                    else if (auto *brInst = dyn_cast<BranchInst>(I))
                    {
                        // If no 'if', 'while', 'for' (only one br basic block)
                        if (brInst->isUnconditional())
                        {
                            errs() << "Br-Simple\n";

                            // Check if br basic block has been already visited
                            updateWorkList(false, brInst->getSuccessor(0), &listRange, &workList);
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

                            // Default new range pairs and value
                            std::pair<int, int> rangeSuccessor0(infMin, infMax);
                            std::pair<int, int> rangeSuccessor1(infMin, infMax);
                            Value *oper = oper0;

                            if (oper0->hasName())
                            {
                                // Select reference to operand0
                                oper = oper0;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    if (pred == ICmpInst::ICMP_SLT)
                                    {
                                        errs() << oper->getName() << " < " << CI->getZExtValue() << "\n";
                                        rangeSuccessor0.second = CI->getZExtValue() - 1;
                                        rangeSuccessor1.first = CI->getZExtValue();
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
                                        errs() << CI->getZExtValue() << " < " << oper->getName() << "\n";
                                        rangeSuccessor0.first = CI->getZExtValue() + 1;
                                        rangeSuccessor1.second = CI->getZExtValue();
                                    }
                                }
                            }

                            // TODO: First sweep ranges are fine.
                            // If you now see on ReMarkable notes, when there is the br back to while.cond
                            // we need to compute %add, since we found a new range for it (so we need to check BinaryOperations).
                            // We must then add the range to the while.cond BB and add it again to the
                            // workList with this new information

                            // Check for new range and add it in case of updates
                            bool isToAdd0 = updateBBRange(brInst->getSuccessor(0), oper, &listRange, rangeSuccessor0);
                            bool isToAdd1 = updateBBRange(brInst->getSuccessor(1), oper, &listRange, rangeSuccessor1);

                            // Check successor0 if already visited
                            updateWorkList(isToAdd0, brInst->getSuccessor(0), &listRange, &workList);
                            updateWorkList(isToAdd1, brInst->getSuccessor(1), &listRange, &workList);
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
                        errs() << "Phi" << phiInst << "\n";
                    }

                    errs() << "\n";
                }

                // Mark basic block as visited
                listRange.insert(std::pair<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>(BB, emptyVector));
            }

            // --- PRINT FOUND RANGES FOR EACH BASIC BLOCK VISITED --- //
            errs() << "--- VALUE-RANGES ---\n";
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

        // Returns true if new range added (i.e. add branch to workList)
        bool updateBBRange(BasicBlock *BB, Value *oper, std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>> *listRange, std::pair<int, int> rangeSuccessor)
        {
            // New range pair
            std::pair<Value *, std::pair<int, int>> rangePair(oper, rangeSuccessor);

            // Get list of reference already saved inside basic block
            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator searchBBIter = listRange->find(BB);

            // Basic block not already visited, add it with the new range attached
            if (searchBBIter == listRange->end())
            {
                errs() << "NEW ADDED!\n";
                std::vector<std::pair<Value *, std::pair<int, int>>> newList{rangePair};
                listRange->insert(std::pair<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>(BB, newList));
                return true;
            }

            // Basic block already visited, and no range yet registered
            std::vector<std::pair<Value *, std::pair<int, int>>> listRangeBB = listRange->find(BB)->second;
            if (listRangeBB.size() == 0)
            {
                errs() << "NEW ADDED AGAIN!\n";
                listRange->find(BB)->second.push_back(rangePair);
                return true;
            }

            // Check if it is already in the list and range changed, if not, return false (i.e. no update)
            for (unsigned i = 0; i < listRangeBB.size(); ++i)
            {
                Value *val = listRangeBB.at(i).first;
                std::pair<int, int> valRange = listRangeBB.at(i).second;

                errs() << val->getName();

                if (val == oper && (valRange.first != rangeSuccessor.first || valRange.second != rangeSuccessor.second))
                {
                    // Already in the list but range has changed, COMBINE the ranges
                    errs() << "ALREADY THERE!\n";
                    std::pair<int, int> newPairRange = combine(valRange, rangeSuccessor);
                    listRange->find(BB)->second.erase(listRange->find(BB)->second.begin() + i);
                    listRange->find(BB)->second.push_back(std::pair<Value *, std::pair<int, int>>(oper, newPairRange));
                    return true;
                }
            }

            return false;
        }

        // Combine two ranges to compute their new value
        std::pair<int, int> combine(std::pair<int, int> range0, std::pair<int, int> range1)
        {
            return std::pair<int, int>(std::min(range0.first, range1.first), std::max(range0.second, range1.second));
        }

        // Given vector of references and new range, checks if value is already present and range values are changed
        bool isVariableNotPresentOrChanged(Value *oper, std::vector<std::pair<Value *, std::pair<int, int>>> *listRangeBB, std::pair<int, int> rangeSuccessor)
        {
            if (listRangeBB->size() == 0)
            {
                // Not present
                return true;
            }

            // Check if it is already in the list and range changed, if not, return false (i.e. no update)
            for (unsigned i = 0; i < listRangeBB->size(); ++i)
            {
                Value *val = listRangeBB->at(i).first;
                std::pair<int, int> valRange = listRangeBB->at(i).second;

                if (val == oper && (valRange.first != rangeSuccessor.first || valRange.second != rangeSuccessor.second))
                {
                    // Already present and changed
                    return true;
                }
                if (val == oper && valRange.first == rangeSuccessor.first && valRange.second == rangeSuccessor.second)
                {
                    // Already present and not changed
                    return false;
                }
            }

            // Not present
            return true;
        }

        // Check if given BasicBlock is already inside the workList
        bool isInWorkList(BasicBlock *next, std::vector<BasicBlock *> *workList)
        {
            return std::find(workList->begin(), workList->end(), next) != workList->end();
        }

        // If isToAdd is true, then range changed, therefore add to list in any case
        void updateWorkList(bool isToAdd, BasicBlock *next, std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>> *listRange, std::vector<BasicBlock *> *workList)
        {
            // Check successor1 if already visited
            std::map<BasicBlock *, std::vector<std::pair<Value *, std::pair<int, int>>>>::iterator it = listRange->find(next);

            // If not already visited, add to workList (do not add if already present)
            if ((isToAdd || it == listRange->end()) && !isInWorkList(next, workList))
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