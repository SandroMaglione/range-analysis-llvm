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
            // --- PLACEHOLDERS/DEFAULTS --- //
            // Create a Null Value reference
            // Assigned as placeholder when no variable reference available
            LLVMContext &context = Func.getContext();
            Value *nullValue = ConstantInt::get(Type::getInt32Ty(context), -1);

            // Max int range (infinity)
            int infMax = std::numeric_limits<int>::max();
            int infMin = std::numeric_limits<int>::min();

            // Create Null range reference
            std::pair<int, int> emptyIntPair(infMin, infMax);
            std::pair<Value *, std::pair<int, int>> emptyPair(nullValue, emptyIntPair);
            // std::vector<std::pair<Value *, std::pair<int, int>>> emptyVector;
            std::map<Value *, std::pair<int, int>> emptyMap;

            // --- DATA STRUCTURES --- //
            // Save cmp instructions to resolve on br instructions
            std::vector<ICmpInst::Predicate> listCmp;
            std::vector<Value *> listCmpOpe0;
            std::vector<Value *> listCmpOpe1;

            // List of basic blocks left to cycle
            std::vector<BasicBlock *> workList;

            // For each basic block, store list of ranges
            // Contains list of value reference and current min and max range for that value
            // {
            //      "BB1": { '%k', { 0, 100 } }
            // }
            std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> listRange;

            // --- ALGORITHM BEGIN --- //
            // Entry basic block into workList (starting point)
            workList.push_back(&Func.getEntryBlock());

            // Loop on the worklist until all dependencies are resolved
            while (workList.size() != 0)
            {
                // Get next BasicBlock in workList and remove it
                BasicBlock *BB = workList.at(0);
                workList.erase(workList.begin());
                errs() << "\n--- " << BB->getName() << " ---\n";

                // Run over all instructions in the basic block
                for (BasicBlock::InstListType::iterator it =
                         BB->getInstList().begin();
                     it != BB->getInstList().end(); ++it)
                {
                    // Get instruction from iterator
                    Instruction *I = &*it;

                    if (auto *operInst = dyn_cast<BinaryOperator>(I))
                    {
                        errs() << "@Operation\n";
                        // Get operands from binary operation
                        Value *oper0 = operInst->getOperand(0);
                        Value *oper1 = operInst->getOperand(1);
                        std::pair<int, int> rangeRef(infMin, infMax);

                        if (ConstantInt *CI0 = dyn_cast<ConstantInt>(oper0))
                        {
                            int constValue0 = CI0->getZExtValue();

                            // a = 1 + 1
                            if (ConstantInt *CI1 = dyn_cast<ConstantInt>(oper1))
                            {
                                int constValue1 = CI1->getZExtValue();
                                int totConst = constValue0 + constValue1;
                                rangeRef.first = totConst;
                                rangeRef.second = totConst;
                            }
                            // a = 1 + b
                            else
                            {
                                std::map<Value *, std::pair<int, int>>::iterator valueRef = getValueReference(BB, oper1, &listRange);
                                if (valueRef->second.first != infMin)
                                {
                                    rangeRef.first = valueRef->second.first + constValue0;
                                }
                                else
                                {
                                    rangeRef.first = infMin;
                                }
                                if (valueRef->second.second != infMax)
                                {
                                    rangeRef.second = valueRef->second.second + constValue0;
                                }
                                else
                                {
                                    rangeRef.second = infMax;
                                }
                            }
                        }
                        else if (ConstantInt *CI1 = dyn_cast<ConstantInt>(oper1))
                        {
                            int constValue1 = CI1->getZExtValue();

                            // a = b + 1
                            if (oper0->hasName())
                            {
                                std::map<Value *, std::pair<int, int>>::iterator valueRef = getValueReference(BB, oper0, &listRange);
                                if (valueRef->second.first != infMin)
                                {
                                    rangeRef.first = valueRef->second.first + constValue1;
                                }
                                else
                                {
                                    rangeRef.first = infMin;
                                }
                                if (valueRef->second.second != infMax)
                                {
                                    rangeRef.second = valueRef->second.second + constValue1;
                                }
                                else
                                {
                                    rangeRef.second = infMax;
                                }
                            }
                        }
                        // a = b + c
                        else
                        {
                            // TODO: Both are references
                        }

                        std::pair<Value *, std::pair<int, int>> rangePair(operInst, rangeRef);
                        errs() << "NEW REFERENCE VALUE: " << operInst->getName() << " into " << BB->getName() << "\n";
                        listRange.find(BB)->second.insert(rangePair);
                    }
                    else if (auto *brInst = dyn_cast<BranchInst>(I))
                    {
                        // If no 'if', 'while', 'for' (only one br basic block)
                        if (brInst->isUnconditional())
                        {
                            errs() << "@Br-Simple\n";
                            BasicBlock *succ = brInst->getSuccessor(0);
                            bool isVisited = isAlreadyVisited(succ, &listRange);

                            if (!isVisited)
                            {
                                listRange.insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(succ, emptyMap));
                            }

                            applySimpleBr(isVisited, succ, BB, &listRange, &workList);
                        }
                        else
                        {
                            errs() << "@Br-Complex\n";

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

                            // Check for new range and add it in case of updates
                            BasicBlock *succ0 = brInst->getSuccessor(0);
                            BasicBlock *succ1 = brInst->getSuccessor(1);

                            bool isUpdated0 = applyComplexBr(succ0, oper, &listRange, rangeSuccessor0);
                            bool isUpdated1 = applyComplexBr(succ1, oper, &listRange, rangeSuccessor1);

                            // Check successor0 if already visited
                            if (isUpdated0)
                            {
                                addToWorklist(succ0, &workList);
                            }
                            if (isUpdated1)
                            {
                                addToWorklist(succ1, &workList);
                            }
                        }
                    }
                    else if (auto *cmpInst = dyn_cast<CmpInst>(I))
                    {
                        errs() << "@Cmp\n";
                        ICmpInst::Predicate pred = cmpInst->getPredicate();
                        listCmp.push_back(pred);
                        listCmpOpe0.push_back(cmpInst->getOperand(0));
                        listCmpOpe1.push_back(cmpInst->getOperand(1));
                    }
                    else if (auto *phiInst = dyn_cast<PHINode>(I))
                    {
                        Value *operand0 = phiInst->getOperand(0);
                        Value *operand1 = phiInst->getOperand(1);

                        errs() << "@Phi: (" << operand0->getName() << ", " << operand1->getName() << ")\n";

                        // If missing reference to value range in current basic block then skip
                        if (operand0->hasName() && !hasValueReference(BB, operand0, &listRange))
                        {
                            errs() << "MISSING: " << operand1->getName() << "\n";
                        }
                        else if (operand1->hasName() && !hasValueReference(BB, operand1, &listRange))
                        {
                            errs() << "MISSING: " << operand1->getName() << "\n";
                        }

                        // TODO: Step 8) of the sketched algorithm
                    }

                    errs() << "\n";
                }

                // If basic block not already visited
                if (!isAlreadyVisited(BB, &listRange))
                {
                    listRange.insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, emptyMap));
                }
            }

            // --- PRINT FOUND RANGES FOR EACH BASIC BLOCK VISITED --- //
            errs() << "--- VALUE-RANGES ---\n";
            std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>>::iterator resIt;
            for (resIt = listRange.begin(); resIt != listRange.end(); ++resIt)
            {
                errs() << "BB: " << resIt->first->getName() << "\n";
                std::map<Value *, std::pair<int, int>> listBB = resIt->second;
                while (!listBB.empty())
                {
                    std::map<Value *, std::pair<int, int>>::iterator pairBB = listBB.begin();
                    errs() << "   " << pairBB->first->getName() << "(" << pairBB->second.first << ", " << pairBB->second.second << ")\n";
                    listBB.erase(listBB.begin());
                }
                errs() << "\n";
            }

            return false;
        }

        // If basic block not already visited and not already inside workList, insert it in workList
        void applySimpleBr(bool wasVisited, BasicBlock *BB, BasicBlock *BBSource, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, std::vector<BasicBlock *> *workList)
        {
            bool isInWL = isInWorkList(BB, workList);
            bool sameRanges = updateToSameRanges(BBSource, BB, listRange);

            if ((!sameRanges || !wasVisited) && !isInWL)
            {
                errs() << "ADDED TO WORKLIST " << BB->getName() << " (sameRanges=" << sameRanges << ", wasVisited=" << wasVisited << ")\n";
                workList->push_back(BB);
            }
        }

        // Returns true if new range added (i.e. add branch to workList)
        bool applyComplexBr(BasicBlock *BB, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, std::pair<int, int> rangeSuccessor)
        {
            // New range pair
            std::pair<Value *, std::pair<int, int>> rangePair(operand, rangeSuccessor);

            // --- CASE 1: BasicBlock not visited --- //
            // Basic block not already visited, add it with the new range attached
            if (!isAlreadyVisited(BB, listRange))
            {
                errs() << "NEW VISITED ADDED: " << BB->getName() << "\n";
                std::map<Value *, std::pair<int, int>> newList{rangePair};
                listRange->insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, newList));
                return true;
            }

            // --- CASE 2: Operand reference missing in the BasicBlock --- //
            // Basic block already visited, and no range yet registered
            if (!hasValueReference(BB, operand, listRange))
            {
                errs() << "NEW REFERENCE ADDED: " << operand->getName() << " into " << BB->getName() << "\n";
                listRange->find(BB)->second.insert(rangePair);
                return true;
            }

            // Get value reference from listRange
            std::map<Value *, std::pair<int, int>>::iterator valueRef = getValueReference(BB, operand, listRange);
            Value *val = valueRef->first;
            std::pair<int, int> valRange = valueRef->second;

            errs() << val->getName();

            // --- CASE 3: Value range changed --- //
            if (valRange.first != rangeSuccessor.first || valRange.second != rangeSuccessor.second)
            {
                // Already in the list but range has changed, COMBINE the ranges
                errs() << "RANGE CHANGED: " << operand->getName() << " from " << BB->getName() << "\n";
                std::pair<int, int> newPairRange = combine(valRange, rangeSuccessor);
                listRange->find(BB)->second.find(operand)->second = newPairRange;
                return true;
            }

            // --- CASE 4: Value range already present and not changed --- //
            return false;
        }

        // Check if two basic block have the same ranges (and combine them if not)
        bool updateToSameRanges(BasicBlock *BBSource, BasicBlock *BBBr, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            bool hasSame = true;

            std::map<Value *, std::pair<int, int>> valuesSource = listRange->find(BBSource)->second;
            std::map<Value *, std::pair<int, int>>::iterator resIt;
            for (resIt = valuesSource.begin(); resIt != valuesSource.end(); ++resIt)
            {
                if (hasValueReference(BBBr, resIt->first, listRange))
                {
                    std::map<Value *, std::pair<int, int>>::iterator valuesBr = getValueReference(BBBr, resIt->first, listRange);
                    if (resIt->second.first != valuesBr->second.first || resIt->second.second != valuesBr->second.second)
                    {
                        // Has value but range is different
                        // TODO: Combine values from both sources
                        hasSame = false;
                    }
                }
                else
                {
                    // Does not have the value
                    std::pair<Value *, std::pair<int, int>> rangePair(resIt->first, resIt->second);
                    listRange->find(BBBr)->second.insert(rangePair);
                    hasSame = false;
                }
            }

            return hasSame;
        }

        // Insert BasicBlock in workList if not already present
        void addToWorklist(BasicBlock *BB, std::vector<BasicBlock *> *workList)
        {
            if (!isInWorkList(BB, workList))
            {
                workList->push_back(BB);
            }
        }

        // Combine two ranges to compute their new value
        std::pair<int, int> combine(std::pair<int, int> range0, std::pair<int, int> range1)
        {
            return std::pair<int, int>(std::min(range0.first, range1.first), std::max(range0.second, range1.second));
        }

        // Given vector of references and new range, checks if value is already present and range values are changed
        bool isVariableNotPresentOrChanged(Value *oper, std::vector<std::pair<Value *, std::pair<int, int>>> *listRangeBB, std::pair<int, int> rangeSuccessor)
        {
            return true;
            // if (listRangeBB->size() == 0)
            // {
            //     // Not present
            //     return true;
            // }

            // // Check if it is already in the list and range changed, if not, return false (i.e. no update)
            // for (unsigned i = 0; i < listRangeBB->size(); ++i)
            // {
            //     Value *val = listRangeBB->at(i).first;
            //     std::pair<int, int> valRange = listRangeBB->at(i).second;

            //     if (val == oper && (valRange.first != rangeSuccessor.first || valRange.second != rangeSuccessor.second))
            //     {
            //         // Already present and changed
            //         return true;
            //     }
            //     if (val == oper && valRange.first == rangeSuccessor.first && valRange.second == rangeSuccessor.second)
            //     {
            //         // Already present and not changed
            //         return false;
            //     }
            // }

            // // Not present
            // return true;
        }

        // Check if given BasicBlock is already visited in listRange
        bool isAlreadyVisited(BasicBlock *next, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            return listRange->find(next) != listRange->end();
        }

        // Check if given BasicBlock is already visited in listRange
        bool hasValueReference(BasicBlock *next, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            if (!isAlreadyVisited(next, listRange))
            {
                return false;
            }

            return listRange->find(next)->second.find(operand) != listRange->find(next)->second.end();
        }
        
        // Get value and its range from listRange
        std::map<Value *, std::pair<int, int>>::iterator getValueReference(BasicBlock *next, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            return listRange->find(next)->second.find(operand);
        }

        // Check if given BasicBlock is already inside the workList
        bool isInWorkList(BasicBlock *next, std::vector<BasicBlock *> *workList)
        {
            return std::find(workList->begin(), workList->end(), next) != workList->end();
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
