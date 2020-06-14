#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Argument.h"

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
            int maxLoops = 15;
            int iterLoops = 0;

            // Create Null range reference
            std::pair<int, int> emptyIntPair(infMin, infMax);
            std::pair<Value *, std::pair<int, int>> emptyPair(nullValue, emptyIntPair);
            std::map<Value *, std::pair<int, int>> emptyMap;

            // --- DATA STRUCTURES --- //
            // Save cmp instructions to resolve on br instructions
            std::map<Value *, CmpInst *> mapCmp;

            // List of basic blocks left to cycle
            std::vector<BasicBlock *> workList;

            // For each basic block, store list of ranges
            // Contains list of value reference and current min and max range for that value
            // {
            //      "BB1": { '%k', { 0, 100 } }
            // }
            std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> listRange;

            // iterator_range<Argument> args = Func.args();
            // for (Argument iter = args.begin(); iter != args.end(); iter++)
            // {
            //     errs() << "Param: " << iter.getName() << "\n";
            // }

            // --- ALGORITHM BEGIN --- //
            // Entry basic block into workList (starting point)
            workList.push_back(&Func.getEntryBlock());

            // Loop on the worklist until all dependencies are resolved
            while (workList.size() != 0 && iterLoops < maxLoops)
            {
                // Get next BasicBlock in workList and remove it
                ++iterLoops;
                BasicBlock *BB = workList.at(0);
                workList.erase(workList.begin());
                errs() << "\n--- (" << iterLoops << ") " << BB->getName() << " ---\n";

                // --- PRINT CURRENT VALUE RANGES INSIDE BLOCK --- //
                if (isAlreadyVisited(BB, &listRange))
                {
                    std::map<Value *, std::pair<int, int>> refList = listRange.find(BB)->second;
                    std::map<Value *, std::pair<int, int>>::iterator resIt;
                    if (refList.begin() == refList.end())
                    {
                        errs() << "___No references\n";
                    }
                    for (resIt = refList.begin(); resIt != refList.end(); ++resIt)
                    {
                        errs() << "___" << resIt->first->getName() << "(" << resIt->second.first << ", " << resIt->second.second << ")\n";
                    }
                    errs() << "\n";
                }
                else
                {
                    errs() << "___No visited\n\n";
                }

                // Run over all instructions in the basic block
                for (BasicBlock::InstListType::iterator it =
                         BB->getInstList().begin();
                     it != BB->getInstList().end(); ++it)
                {
                    // Get instruction from iterator
                    Instruction *I = &*it;
                    if (auto *cmpInst = dyn_cast<CmpInst>(I)) // COMPLETE
                    {
                        errs() << "@Cmp\n";
                        // Cmp information needed only when at least one reference
                        if (cmpInst->getOperand(0)->hasName() || cmpInst->getOperand(1)->hasName())
                        {
                            if (mapCmp.find(cmpInst) == mapCmp.end())
                            {
                                errs() << "NEW: " << cmpInst->getName() << "\n\n";
                                std::pair<Value *, CmpInst *> newCmpInst(cmpInst, cmpInst);
                                mapCmp.insert(newCmpInst);
                            }
                        }
                    }
                    else if (auto *callInst = dyn_cast<CallInst>(I))
                    {
                        errs() << "@Call\n";
                        for (unsigned args = 0; args < callInst->getNumArgOperands(); ++args)
                        {
                            Value *argOper = callInst->getArgOperand(args);
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(argOper))
                            {
                                errs() << "Arg" << args << ": " << CI->getZExtValue() << " (CONST)\n";
                            }
                            else
                            {
                                errs() << "Arg" << args << ": " << argOper->getName() << " (REF)\n";
                            }
                        }
                    }
                    else if (auto *loadInst = dyn_cast<LoadInst>(I))
                    {
                        errs() << "@Load\n";
                        errs() << loadInst->getPointerOperand()->getName() << "\n";
                        errs() << loadInst->getName() << "\n";
                    }
                    else if (auto *selectInst = dyn_cast<SelectInst>(I))
                    {
                        errs() << "@Select\n";
                        errs() << "Condition: " << selectInst->getCondition()->getName() << "\n";
                        errs() << "True: " << selectInst->getTrueValue()->getName() << "\n";
                        errs() << "False: " << selectInst->getFalseValue()->getName() << "\n";
                    }
                    else if (auto *operInst = dyn_cast<BinaryOperator>(I))
                    {
                        errs() << "@Operation\n";
                        // Get operands from binary operation
                        Value *oper0 = operInst->getOperand(0);
                        Value *oper1 = operInst->getOperand(1);
                        unsigned operCode = operInst->getOpcode();
                        std::pair<int, int> rangeRef(infMin, infMax);

                        if (ConstantInt *CI0 = dyn_cast<ConstantInt>(oper0))
                        {
                            int constValue0 = CI0->getZExtValue();

                            // a = 1 + 1 (constant)
                            if (ConstantInt *CI1 = dyn_cast<ConstantInt>(oper1))
                            {
                                int constValue1 = CI1->getZExtValue();
                                int totConst = binaryOperationResult(operCode, constValue0, constValue1);
                                rangeRef.first = totConst;
                                rangeRef.second = totConst;
                            }
                            // a = 1 + b
                            else
                            {
                                std::pair<int, int> valueRef = getValueReference(BB, oper1, &listRange, infMin, infMax)->second;
                                errs() << operInst->getName() << " = " << oper1->getName() << " (" << valueRef.first << ", " << valueRef.second << ") | " << constValue0 << " [" << BB->getName() << "]\n";

                                if (valueRef.first != infMin)
                                {
                                    rangeRef.first = binaryOperationResult(operCode, constValue0, valueRef.first);
                                }
                                else
                                {
                                    rangeRef.first = infMin;
                                }
                                if (valueRef.second != infMax)
                                {
                                    rangeRef.second = binaryOperationResult(operCode, constValue0, valueRef.second);
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
                                std::pair<int, int> valueRef = getValueReference(BB, oper0, &listRange, infMin, infMax)->second;
                                errs() << operInst->getName() << " = " << oper0->getName() << " (" << valueRef.first << ", " << valueRef.second << ") | " << constValue1 << " [" << BB->getName() << "]\n";

                                if (valueRef.first != infMin)
                                {
                                    rangeRef.first = binaryOperationResult(operCode, valueRef.first, constValue1);
                                }
                                else
                                {
                                    rangeRef.first = infMin;
                                }
                                if (valueRef.second != infMax)
                                {
                                    rangeRef.second = binaryOperationResult(operCode, valueRef.second, constValue1);
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
                            // a = b + c
                            if (oper0->hasName() && oper1->hasName())
                            {
                                errs() << "BOTH REF: " << oper0->getName() << ", " << oper1->getName() << "\n";
                            }
                            // a = b + [%0]
                            else if (oper0->hasName())
                            {
                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    errs() << "BOTH REF: " << oper0->getName() << ", " << CI->getZExtValue() << "\n";
                                }
                            }
                            // a = [%0] + b
                            else if (oper1->hasName())
                            {
                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                                {
                                    errs() << "BOTH REF: " << CI->getZExtValue() << ", " << oper1->getName() << "\n";
                                }
                            }
                            // a = [%0] + [%1]
                            else
                            {
                                if (ConstantInt *CI0 = dyn_cast<ConstantInt>(oper0))
                                {
                                    if (ConstantInt *CI1 = dyn_cast<ConstantInt>(oper0))
                                    {
                                        errs() << "BOTH REF: " << CI0->getZExtValue() << ", " << CI1->getZExtValue() << "\n";
                                    }
                                }
                            }
                        }

                        std::pair<Value *, std::pair<int, int>> rangePair(operInst, rangeRef);
                        errs() << "NEW: " << operInst->getName() << " (" << rangeRef.first << ", " << rangeRef.second << ") into " << BB->getName() << "\n";
                        if (hasValueReference(BB, operInst, &listRange))
                        {
                            listRange.find(BB)->second.find(operInst)->second.first = rangeRef.first;
                            listRange.find(BB)->second.find(operInst)->second.second = rangeRef.second;
                        }
                        else
                        {
                            listRange.find(BB)->second.insert(rangePair);
                        }
                    }
                    else if (auto *brInst = dyn_cast<BranchInst>(I))
                    {
                        // If no 'if', 'while', 'for' (only one br basic block)
                        if (brInst->isUnconditional())
                        {
                            errs() << "@Br-Simple\n";
                            BasicBlock *succ = brInst->getSuccessor(0);
                            applySimpleBr(true, false, succ, BB, &listRange, &workList, emptyMap, infMin, infMax);
                        }
                        else
                        {
                            errs() << "@Br-Complex\n";
                            errs() << "Condition: " << brInst->getCondition()->getName() << "\n\n";
                            BasicBlock *succ0 = brInst->getSuccessor(0);
                            BasicBlock *succ1 = brInst->getSuccessor(1);

                            // Get previous cmp values
                            CmpInst *cmpInst = mapCmp.find(brInst->getCondition())->second;
                            ICmpInst::Predicate pred = cmpInst->getPredicate();
                            Value *oper0 = cmpInst->getOperand(0);
                            Value *oper1 = cmpInst->getOperand(1);

                            // Default new range pairs and value
                            std::pair<int, int> rangeSuccessor0(infMin, infMax);
                            std::pair<int, int> rangeSuccessor1(infMin, infMax);
                            Value *oper = oper0;

                            // a < b
                            if (oper0->hasName() && oper1->hasName())
                            {
                                // TODO: Cmp instruction both reference values
                            }
                            // a < 1
                            else if (oper0->hasName())
                            {
                                // Select reference to operand0
                                oper = oper0;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    // Change range of successors based on reference, constant value, and predicate
                                    computeCmpRange(true, pred, oper, CI->getZExtValue(), &rangeSuccessor0, &rangeSuccessor1);
                                }
                            }
                            // 1 < a
                            else
                            {
                                // Select reference to operand0
                                oper = oper1;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                                {
                                    // Change range of successors based on reference, constant value, and predicate
                                    computeCmpRange(false, pred, oper, CI->getZExtValue(), &rangeSuccessor0, &rangeSuccessor1);
                                }
                            }

                            if (hasValueReference(BB, oper, &listRange))
                            {
                                std::map<Value *, std::pair<int, int>>::iterator valRef = getValueReference(BB, oper, &listRange, infMin, infMax);
                                rangeSuccessor0.first = std::max(rangeSuccessor0.first, valRef->second.first);
                                rangeSuccessor0.second = std::min(rangeSuccessor0.second, valRef->second.second);
                                rangeSuccessor1.first = std::max(rangeSuccessor1.first, valRef->second.first);
                                rangeSuccessor1.second = std::min(rangeSuccessor1.second, valRef->second.second);
                            }

                            // Check for new range and add it in case of updates
                            errs() << succ0->getName() << ": (" << rangeSuccessor0.first << ", " << rangeSuccessor0.second << ")\n";
                            errs() << succ1->getName() << ": (" << rangeSuccessor1.first << ", " << rangeSuccessor1.second << ")\n";

                            bool isUpdated0 = applyComplexBr(succ0, oper, &listRange, rangeSuccessor0, infMin, infMax);
                            bool isUpdated1 = applyComplexBr(succ1, oper, &listRange, rangeSuccessor1, infMin, infMax);

                            // Check successor0 if already visited
                            applySimpleBr(false, isUpdated0, succ0, BB, &listRange, &workList, emptyMap, infMin, infMax);
                            applySimpleBr(false, isUpdated1, succ1, BB, &listRange, &workList, emptyMap, infMin, infMax);
                        }
                    }
                    else if (auto *phiInst = dyn_cast<PHINode>(I)) // COMPLETE?
                    {
                        // General ranges and values
                        Value *operand0 = phiInst->getOperand(0);
                        Value *operand1 = phiInst->getOperand(1);
                        std::map<Value *, std::pair<int, int>>::iterator valRefSource = getValueReference(BB, phiInst, &listRange, infMin, infMax);
                        std::pair<int, int> phiPair(infMin, infMax);

                        errs() << "@Phi: " << phiInst->getName() << " (" << operand0->getName() << ", " << operand1->getName() << ")\n";

                        // Both referenced values
                        if (operand0->hasName() && operand1->hasName())
                        {
                            std::map<Value *, std::pair<int, int>>::iterator valRef0 = getValueReference(BB, operand0, &listRange, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef1 = getValueReference(BB, operand1, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            phiPair = phiOpe(valRefSource->second, valRef0->second, valRef1->second);
                        }
                        // Operator1 is known reference, Operator0 is constant
                        else if (!operand0->hasName() && operand1->hasName())
                        {
                            std::pair<int, int> constPair = getConstantPair(operand0, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef = getValueReference(BB, operand1, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            phiPair = phiOpe(valRefSource->second, constPair, valRef->second);
                        }
                        // Operator0 is known reference, Operator1 is constant
                        else if (!operand1->hasName() && operand0->hasName() && hasValueReference(BB, operand0, &listRange))
                        {
                            std::pair<int, int> constPair = getConstantPair(operand1, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef = getValueReference(BB, operand0, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            phiPair = phiOpe(valRefSource->second, valRef->second, constPair);
                        }
                        // Both integers
                        else if (!operand0->hasName() && !operand1->hasName())
                        {
                            // TODO: Phi of two constant values (is it possible?)
                            errs() << "\n\nTWO INTEGERS\n\n";
                        }
                        else
                        {
                            errs() << "\n\nUNEXPECTED SITUATION!\n\n";
                        }

                        // Update/Insert new phi range to the value in the current basic block
                        updateValueReference(BB, phiInst, phiPair, &listRange);
                        errs() << "New Range: (" << phiPair.first << ", " << phiPair.second << ")\n";
                    }

                    errs() << "\n";
                }

                // If basic block not already visited, mark as visited
                if (!isAlreadyVisited(BB, &listRange))
                {
                    listRange.insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, emptyMap));
                }
            }

            // --- PRINT FOUND RANGES FOR EACH BASIC BLOCK VISITED --- //
            if (iterLoops == maxLoops)
            {
                errs() << "--- (MAX ITERATIONS LIMIT) ---\n";
            }
            errs() << "--- VALUE-RANGES ---\n";
            std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>>::iterator resIt;
            for (resIt = listRange.begin(); resIt != listRange.end(); ++resIt)
            {
                errs() << "BB: " << resIt->first->getName() << "\n";
                std::map<Value *, std::pair<int, int>> listBB = resIt->second;
                while (!listBB.empty())
                {
                    std::map<Value *, std::pair<int, int>>::iterator pairBB = listBB.begin();
                    int intRange = std::abs(pairBB->second.second - pairBB->second.first) + 1;
                    int numOfBit = std::ceil(intRange <= 2 ? 1 : std::log2(intRange)) + 1;
                    errs() << "   " << pairBB->first->getName() << "(" << pairBB->second.first << ", " << pairBB->second.second << ") = " << intRange << " is " << numOfBit << "bits\n";
                    listBB.erase(listBB.begin());
                }
                errs() << "\n";
            }

            return false;
        }

        // If basic block not already visited and not already inside workList, insert it in workList
        void applySimpleBr(bool isBrSimple, bool isUpdated, BasicBlock *BB, BasicBlock *BBSource, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, std::vector<BasicBlock *> *workList, std::map<Value *, std::pair<int, int>> emptyMap, int infMin, int infMax)
        {
            bool isVisited = isAlreadyVisited(BB, listRange);
            if (!isVisited)
            {
                listRange->insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, emptyMap));
            }

            bool isInWL = isInWorkList(BB, workList);
            bool sameRanges = true;
            if (isBrSimple)
            {
                sameRanges = updateToSameRanges(BBSource, BB, listRange, infMin, infMax);
            }

            if ((!sameRanges || !isVisited || isUpdated) && !isInWL)
            {
                errs() << "+ " << BB->getName() << " (sameRanges=" << !sameRanges << ", wasVisited=" << !isVisited << ", isUpdated=" << isUpdated << ")\n";
                workList->push_back(BB);
            }
        }

        // Returns true if new range added (i.e. add branch to workList)
        bool applyComplexBr(BasicBlock *BB, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, std::pair<int, int> rangeSuccessor, int infMin, int infMax)
        {
            // New range pair
            std::pair<Value *, std::pair<int, int>> rangePair(operand, rangeSuccessor);

            // --- CASE 1: BasicBlock not visited --- //
            // Basic block not already visited, add it with the new range attached
            if (!isAlreadyVisited(BB, listRange))
            {
                // errs() << "NEW VISITED ADDED: " << BB->getName() << "\n";
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
            std::map<Value *, std::pair<int, int>>::iterator valueRef = getValueReference(BB, operand, listRange, infMin, infMax);
            Value *val = valueRef->first;
            std::pair<int, int> valRange = valueRef->second;

            errs() << "-" << val->getName() << ": (" << valRange.first << ", " << valRange.second << ") to (" << rangeSuccessor.first << ", " << rangeSuccessor.second << ")\n";

            // --- CASE 3: Value range changed --- //
            if (valRange.first != rangeSuccessor.first || valRange.second != rangeSuccessor.second)
            {
                // Already in the list but range has changed, COMBINE the ranges
                std::pair<int, int> newPairRange = phiCombine(valRange, rangeSuccessor, infMin, infMax);
                errs() << "NEW: " << operand->getName() << " (" << newPairRange.first << ", " << newPairRange.second << ") in " << BB->getName() << "\n\n";
                listRange->find(BB)->second.find(operand)->second = newPairRange;
                return true;
            }

            // --- CASE 4: Value range already present and not changed --- //
            return false;
        }

        // Check if two basic block have the same ranges (and combine them if not)
        bool updateToSameRanges(BasicBlock *BBSource, BasicBlock *BBBr, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, int infMin, int infMax)
        {
            bool hasSame = true;
            errs() << BBSource->getName() << " to " << BBBr->getName() << "\n";
            std::map<Value *, std::pair<int, int>> valuesSource = listRange->find(BBSource)->second;
            std::map<Value *, std::pair<int, int>>::iterator resIt;
            for (resIt = valuesSource.begin(); resIt != valuesSource.end(); ++resIt)
            {
                if (hasValueReference(BBBr, resIt->first, listRange))
                {
                    std::map<Value *, std::pair<int, int>>::iterator valuesBr = getValueReference(BBBr, resIt->first, listRange, infMin, infMax);
                    errs() << resIt->first->getName() << ": " << BBSource->getName() << " (" << resIt->second.first << ", " << resIt->second.second << ") != " << BBBr->getName() << " (" << valuesBr->second.first << ", " << valuesBr->second.second << ") ?\n";
                    if (resIt->second.first != valuesBr->second.first || resIt->second.second != valuesBr->second.second)
                    {
                        // Has value but range is different
                        std::pair<int, int> phiPair = phiCombine(resIt->second, valuesBr->second, infMin, infMax);
                        errs() << "NEW! -> (" << phiPair.first << ", " << phiPair.second << ")\n";
                        listRange->find(BBBr)->second.find(resIt->first)->second.first = phiPair.first;
                        listRange->find(BBBr)->second.find(resIt->first)->second.second = phiPair.second;
                        hasSame = false;
                    }
                }
                else
                {
                    // Does not have the value
                    std::pair<Value *, std::pair<int, int>> rangePair(resIt->first, resIt->second);
                    listRange->find(BBBr)->second.insert(rangePair);
                    errs() << "NEW: " << resIt->first->getName() << " (" << resIt->second.first << ", " << resIt->second.second << ")\n";
                    hasSame = false;
                }
            }

            return hasSame;
        }

        // Min of minimum values, max of maximum values
        std::pair<int, int> unionOpe(std::pair<int, int> range0, std::pair<int, int> range1)
        {
            return std::pair<int, int>(std::min(range0.first, range1.first), std::max(range0.second, range1.second));
        }

        // Max of minimum values, min of maximum values
        std::pair<int, int> interOpe(std::pair<int, int> range0, std::pair<int, int> range1)
        {
            return std::pair<int, int>(std::max(range0.first, range1.first), std::min(range0.second, range1.second));
        }

        // Range combining operation for phi instructions
        std::pair<int, int> phiOpe(std::pair<int, int> rangeOriginal, std::pair<int, int> rangeSource0, std::pair<int, int> rangeSource1)
        {
            return unionOpe(rangeOriginal, interOpe(rangeSource0, rangeSource1));
        }

        // Range combining operation for br-complex instructions
        std::pair<int, int> brOpe(std::pair<int, int> rangeSource, std::pair<int, int> rangeDestination, std::pair<int, int> rangeBranch)
        {
            return interOpe(rangeBranch, interOpe(rangeSource, rangeDestination));
        }

        // Combine two ranges to compute their new value
        std::pair<int, int> phiCombine(std::pair<int, int> range0, std::pair<int, int> range1, int infMin, int infMax)
        {
            if ((range0.first == infMin && range0.second == infMax) || (range1.first == infMin && range1.second == infMax))
            {
                return std::pair<int, int>(infMin, infMax);
            }
            else if (range0.first >= range1.first && range0.second >= range1.first && range0.first >= range1.second && range0.second >= range1.second)
            {
                int maxVal = std::max(range0.first, range0.second);
                if (range0.first == infMax)
                {
                    maxVal = range0.second;
                }
                else if (range0.second == infMax)
                {
                    maxVal = range0.first;
                }
                return std::pair<int, int>(maxVal, maxVal);
            }
            else if (range1.first >= range0.first && range1.second >= range0.first && range1.first >= range0.second && range1.second >= range0.second)
            {
                int maxVal = std::max(range1.first, range1.second);
                if (range1.first == infMax)
                {
                    maxVal = range1.second;
                }
                else if (range1.second == infMax)
                {
                    maxVal = range1.first;
                }
                return std::pair<int, int>(maxVal, maxVal);
            }

            return std::pair<int, int>(
                range0.first == infMin ? range1.first : range1.first == infMin ? range0.first : std::min(range0.first, range1.first),
                range0.second == infMax ? range1.second : range1.second == infMax ? range0.second : std::max(range0.second, range1.second));
        }

        // Compute binary operation (+ or -) result
        int binaryOperationResult(unsigned binOper, int value1, int value2)
        {
            if (binOper == Instruction::Add)
            {
                return value1 + value2;
            }
            else if (binOper == Instruction::Sub)
            {
                return value1 - value2;
            }

            return value1 + value2;
        }

        // Computes ranges from CMP instruction (<, >, <=, >=)
        void computeCmpRange(bool isRefOper0, ICmpInst::Predicate pred, Value *oper, int cmpValue, std::pair<int, int> *rangeSuccessor0, std::pair<int, int> *rangeSuccessor1)
        {
            if (pred == ICmpInst::ICMP_SLT)
            {
                // a < 1
                if (isRefOper0)
                {
                    errs() << oper->getName() << " < " << cmpValue << "\n";
                    rangeSuccessor0->second = cmpValue - 1;
                    rangeSuccessor1->first = cmpValue;
                }
                // 1 < a
                else
                {
                    errs() << cmpValue << " < " << oper->getName() << "\n";
                    rangeSuccessor0->first = cmpValue + 1;
                    rangeSuccessor1->second = cmpValue;
                }
            }
            else if (pred == ICmpInst::ICMP_ULT)
            {
                // a < 1 [unsigned]
                if (isRefOper0)
                {
                    errs() << oper->getName() << " < " << cmpValue << " [unsigned]\n";
                    rangeSuccessor0->first = 0;
                    rangeSuccessor0->second = cmpValue - 1;
                    rangeSuccessor1->first = cmpValue;
                }
                // 1 < a (a > 1) [unsigned]
                else
                {
                    errs() << cmpValue << " < " << oper->getName() << " [unsigned]\n";
                    rangeSuccessor0->first = cmpValue + 1;
                    rangeSuccessor1->first = 0;
                    rangeSuccessor1->second = cmpValue;
                }
            }
            else if (pred == ICmpInst::ICMP_SLE)
            {
                // a <= 1
                if (isRefOper0)
                {
                    errs() << oper->getName() << " <= " << cmpValue << "\n";
                    rangeSuccessor0->second = cmpValue;
                    rangeSuccessor1->first = cmpValue + 1;
                }
                // 1 <= a
                else
                {
                    errs() << cmpValue << " <= " << oper->getName() << "\n";
                    rangeSuccessor0->first = cmpValue;
                    rangeSuccessor1->second = cmpValue - 1;
                }
            }
            else if (pred == ICmpInst::ICMP_SGT)
            {
                // a > 1
                if (isRefOper0)
                {
                    errs() << oper->getName() << " > " << cmpValue << "\n";
                    rangeSuccessor0->first = cmpValue + 1;
                    rangeSuccessor1->second = cmpValue;
                }
                // 1 > a
                else
                {
                    errs() << cmpValue << " > " << oper->getName() << "\n";
                    rangeSuccessor0->second = cmpValue - 1;
                    rangeSuccessor1->first = cmpValue;
                }
            }
            else if (pred == ICmpInst::ICMP_UGT)
            {
                // a > 1
                if (isRefOper0)
                {
                    errs() << oper->getName() << " > " << cmpValue << "\n";
                    rangeSuccessor0->first = cmpValue + 1;
                    rangeSuccessor1->first = 0;
                    rangeSuccessor1->second = cmpValue;
                }
                // 1 > a
                else
                {
                    errs() << cmpValue << " > " << oper->getName() << "\n";
                    rangeSuccessor0->first = cmpValue - 1;
                    rangeSuccessor0->second = cmpValue - 1;
                    rangeSuccessor1->first = cmpValue;
                }
            }
            else if (pred == ICmpInst::ICMP_SGE)
            {
                // a >= 1
                if (isRefOper0)
                {
                    errs() << oper->getName() << " >= " << cmpValue << "\n";
                    rangeSuccessor0->first = cmpValue;
                    rangeSuccessor1->second = cmpValue - 1;
                }
                // 1 >= a
                else
                {
                    errs() << cmpValue << " >= " << oper->getName() << "\n";
                    rangeSuccessor0->second = cmpValue;
                    rangeSuccessor1->first = cmpValue + 1;
                }
            }
            else if (pred == ICmpInst::ICMP_EQ)
            {
                // a == 1
                errs() << oper->getName() << " == " << cmpValue << "\n";
                rangeSuccessor0->first = cmpValue;
                rangeSuccessor0->second = cmpValue;
            }
        }

        // Check if given BasicBlock is already visited in listRange
        bool isAlreadyVisited(BasicBlock *next, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            return listRange->find(next) != listRange->end();
        }

        // Update or insert new Value/Pair into basic block
        void updateValueReference(BasicBlock *BB, Value *operand, std::pair<int, int> pairRange, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            if (hasValueReference(BB, operand, listRange))
            {
                // Update reference
                listRange->find(BB)->second.find(operand)->second.first = pairRange.first;
                listRange->find(BB)->second.find(operand)->second.second = pairRange.second;
            }
            else
            {
                // Insert reference
                std::pair<Value *, std::pair<int, int>> newPairRange(operand, pairRange);
                listRange->find(BB)->second.insert(newPairRange);
            }
        }

        // Check if given BasicBlock is already visited in listRange
        bool hasValueReference(BasicBlock *next, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange)
        {
            // No reference if never visited
            if (!isAlreadyVisited(next, listRange))
            {
                return false;
            }

            return listRange->find(next)->second.find(operand) != listRange->find(next)->second.end();
        }

        // Get value and its range from listRange
        std::map<Value *, std::pair<int, int>>::iterator getValueReference(BasicBlock *BB, Value *operand, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, int infMin, int infMax)
        {
            if (hasValueReference(BB, operand, listRange))
            {
                return listRange->find(BB)->second.find(operand);
            }

            // Unknown variables from -Inf to +Inf
            std::pair<int, int> emptyIntPair(infMin, infMax);
            std::map<Value *, std::pair<int, int>> emptyPair;
            emptyPair[operand] = emptyIntPair;
            return emptyPair.begin();
        }

        std::pair<int, int> getConstantPair(Value *operand, int infMin, int infMax)
        {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(operand))
            {
                int constValue = CI->getZExtValue();
                return std::pair<int, int>(constValue, constValue);
            }

            // Unknown variables from -Inf to +Inf
            errs() << "\n\nEXPECTED CONSTANT IS NOT ACTUALLY CONSTANT!\n\n";
            return std::pair<int, int>(infMin, infMax);
        }

        // Check if given BasicBlock is already inside the workList
        bool isInWorkList(BasicBlock *next, std::vector<BasicBlock *> *workList)
        {
            return std::find(workList->begin(), workList->end(), next) != workList->end();
        }
    }; // namespace
} // end of anonymous namespace

char HppsBranchRange::ID = 0;
static RegisterPass<HppsBranchRange> X("branch-range", "Branch Range Pass",
                                       false /* Only looks at CFG */,
                                       false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new HppsBranchRange()); });
