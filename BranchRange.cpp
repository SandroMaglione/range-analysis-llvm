#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/CFG.h"

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
            int maxLoops = 1000;
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
                bool hasBeenUpdated = false;
                ++iterLoops;
                BasicBlock *BB = workList.at(0);
                workList.erase(workList.begin());
                errs() << "\n--- (" << iterLoops << ") " << BB->getName() << " ---\n";

                // --- PRINT ALL PREDECESSORS --- //
                for (BasicBlock *Pred : predecessors(BB))
                {
                    errs() << "..." << Pred->getName() << "\n";
                }

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
                        errs() << "___" << resIt->first->getName() << printRange(resIt->second, infMin, infMax) << "\n";
                    }
                    errs() << "\n";
                }
                else
                {
                    errs() << "___No visited\n\n";
                }

                // If basic block not already visited, mark as visited
                if (!isAlreadyVisited(BB, &listRange))
                {
                    listRange.insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, emptyMap));
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
                            if (argOper->hasName())
                            {
                                errs() << "Unknown range on " << argOper->getName() << "(" << args << ")\n";
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
                                errs() << operInst->getName() << " = " << oper1->getName() << printRange(valueRef, infMin, infMax) << " | " << constValue0 << " [" << BB->getName() << "]\n";

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
                                errs() << operInst->getName() << " = " << oper0->getName() << printRange(valueRef, infMin, infMax) << " | " << constValue1 << " [" << BB->getName() << "]\n";

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
                        errs() << "NEW: " << operInst->getName() << printRange(rangeRef, infMin, infMax) << "\n";
                        if (hasValueReference(BB, operInst, &listRange))
                        {
                            // Add to worklist if range has been updated
                            std::pair<int, int> valRefSource = getValueReference(BB, operInst, &listRange, infMin, infMax)->second;
                            hasBeenUpdated = valRefSource.first != rangeRef.first || valRefSource.second != rangeRef.second;

                            listRange.find(BB)->second.find(operInst)->second.first = rangeRef.first;
                            listRange.find(BB)->second.find(operInst)->second.second = rangeRef.second;
                        }
                        else
                        {
                            // Added value reference, add basic block in worklist
                            hasBeenUpdated = true;
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

                            for (BasicBlock *Pred : predecessors(BB))
                            {
                                if (Pred == succ)
                                {
                                    errs() << "LOOP on " << Pred->getName() << "\n";
                                }
                            }

                            applySimpleBr(hasBeenUpdated, succ, &listRange, &workList, emptyMap);
                        }
                        else
                        {
                            errs() << "@Br-Complex\n";
                            errs() << "Condition: " << brInst->getCondition()->getName() << "\n\n";

                            // Successor basic blocks (taken and not taken)
                            BasicBlock *succ0 = brInst->getSuccessor(0);
                            BasicBlock *succ1 = brInst->getSuccessor(1);

                            // Get previous cmp values
                            CmpInst *cmpInst = mapCmp.find(brInst->getCondition())->second;
                            ICmpInst::Predicate pred = cmpInst->getPredicate();
                            Value *oper0 = cmpInst->getOperand(0);
                            Value *oper1 = cmpInst->getOperand(1);

                            // Default new range pairs and value
                            // VAL1: Range of the cmp instruction for branch taken
                            std::pair<int, int> rangeCmpTaken(infMin, infMax);
                            // VAL2: Range of the cmp instruction for branch not taken
                            std::pair<int, int> rangeCmpNotTaken(infMin, infMax);
                            Value *oper = oper0;

                            // a < b
                            if (oper0->hasName() && oper1->hasName())
                            {
                                errs() << "\n\nUNEXPECTED DOUBLE REFERENCE CMP INSTRUCTION\n\n";
                            }
                            // a < 1
                            else if (oper0->hasName())
                            {
                                // Select reference to operand0
                                oper = oper0;

                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                {
                                    // Change range of successors based on reference, constant value, and predicate
                                    computeCmpRange(true, pred, oper, CI->getZExtValue(), &rangeCmpTaken, &rangeCmpNotTaken);
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
                                    computeCmpRange(false, pred, oper, CI->getZExtValue(), &rangeCmpTaken, &rangeCmpNotTaken);
                                }
                            }

                            // VAL3: Range in current basic block of the variable in the cmp instruction
                            std::map<Value *, std::pair<int, int>>::iterator valRefSource = getValueReference(BB, oper, &listRange, infMin, infMax);
                            // VAL4: Range in taken basic block of the variable in the cmp instruction
                            std::map<Value *, std::pair<int, int>>::iterator valBranchTaken = getValueReference(succ0, oper, &listRange, infMin, infMax);
                            // VAL5: Range in not taken basic block of the variable in the cmp instruction
                            std::map<Value *, std::pair<int, int>>::iterator valBranchNotTaken = getValueReference(succ1, oper, &listRange, infMin, infMax);

                            // Final computed branch ranges
                            std::pair<int, int> rangeBranchTaken = brOpe(valRefSource->second, valBranchTaken->second, rangeCmpTaken);
                            std::pair<int, int> rangeBranchNotTaken = brOpe(valRefSource->second, valBranchNotTaken->second, rangeCmpNotTaken);

                            errs() << valRefSource->first->getName() << printRange(valRefSource->second, infMin, infMax) << " "
                                   << valBranchTaken->first->getName() << printRange(valBranchTaken->second, infMin, infMax) << " "
                                   << printRange(rangeCmpTaken, infMin, infMax) << "\n";

                            errs() << valRefSource->first->getName() << printRange(valRefSource->second, infMin, infMax) << " "
                                   << valBranchNotTaken->first->getName() << printRange(valBranchNotTaken->second, infMin, infMax) << " "
                                   << printRange(rangeCmpNotTaken, infMin, infMax) << "\n";

                            // Check for new range and add it in case of updates
                            errs() << succ0->getName() << ": " << printRange(rangeBranchTaken, infMin, infMax) << "\n";
                            errs() << succ1->getName() << ": " << printRange(rangeBranchNotTaken, infMin, infMax) << "\n\n";

                            // Check successor0 if already visited
                            applySimpleBr(true, succ0, &listRange, &workList, emptyMap);
                            applySimpleBr(true, succ1, &listRange, &workList, emptyMap);

                            // Update/Insert new range in successors basic blocks
                            updateValueReference(succ0, oper, rangeBranchTaken, &listRange, infMin, infMax);
                            updateValueReference(succ1, oper, rangeBranchNotTaken, &listRange, infMin, infMax);
                        }
                    }
                    else if (auto *phiInst = dyn_cast<PHINode>(I))
                    {
                        // General ranges and values
                        Value *operand0 = phiInst->getOperand(0);
                        Value *operand1 = phiInst->getOperand(1);

                        BasicBlock *BB0 = phiInst->getIncomingBlock(0);
                        BasicBlock *BB1 = phiInst->getIncomingBlock(1);

                        int search0 = searchInBasicBlock(phiInst, BB0, operand0);
                        int search1 = searchInBasicBlock(phiInst, BB1, operand1);

                        // Range of the phi variable in current basic block
                        std::map<Value *, std::pair<int, int>>::iterator valRefSource = getValueReference(BB, phiInst, &listRange, infMin, infMax);
                        std::pair<int, int> phiPair(infMin, infMax);

                        errs() << "@Phi: " << phiInst->getName() << " (" << operand0->getName() << "[" << BB0->getName() << "], " << operand1->getName() << " [" << BB1->getName() << "])\n";

                        // Both referenced values
                        if (operand0->hasName() && operand1->hasName())
                        {
                            std::map<Value *, std::pair<int, int>>::iterator valRef0 = getValueReference(BB0, operand0, &listRange, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef1 = getValueReference(BB1, operand1, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            errs() << valRefSource->first->getName() << printRange(valRefSource->second, infMin, infMax) << " "
                                   << valRef0->first->getName() << printRange(valRef0->second, infMin, infMax) << " "
                                   << valRef1->first->getName() << printRange(valRef1->second, infMin, infMax) << "\n";

                            phiPair = phiOpe(valRefSource->second, valRef0->second, valRef1->second);
                        }
                        // Operator1 is known reference, Operator0 is constant
                        else if (!operand0->hasName() && operand1->hasName())
                        {
                            std::pair<int, int> constPair = getConstantPair(operand0, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef = getValueReference(BB1, operand1, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            errs() << valRefSource->first->getName() << printRange(valRefSource->second, infMin, infMax) << " "
                                   << printRange(constPair, infMin, infMax) << " "
                                   << valRef->first->getName() << printRange(valRef->second, infMin, infMax) << "\n";

                            phiPair = phiOpe(valRefSource->second, constPair, valRef->second);

                            // Check if the constant operand is the minimum or maximum value
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(operand0))
                            {
                                int constRangeVal = CI->getZExtValue();

                                if (search1 == 1)
                                {
                                    phiPair.first = constRangeVal;
                                    maxTripcount(&phiPair, constRangeVal, phiInst, BB1, operand1, &mapCmp, &listRange, infMin, infMax);
                                }
                                else if (search1 == 2)
                                {
                                    phiPair.second = constRangeVal;
                                    maxTripcount(&phiPair, constRangeVal, phiInst, BB1, operand1, &mapCmp, &listRange, infMin, infMax);
                                }
                            }
                        }
                        // Operator0 is known reference, Operator1 is constant
                        else if (!operand1->hasName() && operand0->hasName())
                        {
                            std::pair<int, int> constPair = getConstantPair(operand1, infMin, infMax);
                            std::map<Value *, std::pair<int, int>>::iterator valRef = getValueReference(BB0, operand0, &listRange, infMin, infMax);

                            // Apply phi combine operation
                            errs() << valRefSource->first->getName() << printRange(valRefSource->second, infMin, infMax) << " "
                                   << valRef->first->getName() << printRange(valRef->second, infMin, infMax) << " "
                                   << printRange(constPair, infMin, infMax) << "\n";

                            phiPair = phiOpe(valRefSource->second, valRef->second, constPair);

                            // Check if the constant operand is the minimum or maximum value
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(operand1))
                            {
                                int constRangeVal = CI->getZExtValue();

                                if (search0 == 1)
                                {
                                    phiPair.first = constRangeVal;
                                    maxTripcount(&phiPair, constRangeVal, phiInst, BB0, operand0, &mapCmp, &listRange, infMin, infMax);
                                }
                                else if (search0 == 2)
                                {
                                    phiPair.second = constRangeVal;
                                    maxTripcount(&phiPair, constRangeVal, phiInst, BB0, operand0, &mapCmp, &listRange, infMin, infMax);
                                }
                            }
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
                        if (phiInst->hasName())
                        {
                            updateValueReference(BB, phiInst, phiPair, &listRange, infMin, infMax);
                        }
                    }

                    errs() << "\n";
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
                    int intRange = pairBB->second.first == infMin || pairBB->second.second == infMax ? infMax : std::abs(pairBB->second.second - pairBB->second.first) + 1;
                    int numOfBit = intRange == infMax ? 32 : std::ceil(intRange <= 2 ? 1 : std::log2(intRange)) + 1;
                    errs() << "   " << pairBB->first->getName() << printRange(pairBB->second, infMin, infMax) << " = ";

                    if (pairBB->second.first != infMin && pairBB->second.second != infMax)
                    {
                        errs() << intRange << " {" << numOfBit << "bit}\n";
                    }
                    else
                    {
                        errs() << "MAX\n";
                    }
                    listBB.erase(listBB.begin());
                }
                errs() << "\n";
            }

            return false;
        }

        // Compute and update maximum range of value add/sub in a loop
        void maxTripcount(std::pair<int, int> *tripPair, int baseVal, Value *inst, BasicBlock *BB, Value *operand, std::map<Value *, CmpInst *> *mapCmp, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, int infMin, int infMax)
        {
            int tripcount = -1;

            for (BasicBlock::InstListType::iterator loopIt =
                     BB->getInstList().begin();
                 loopIt != BB->getInstList().end(); ++loopIt)
            {
                Instruction *loopI = &*loopIt;

                // Search br instruction, if successor is same as predcessor, then is a loop
                if (auto *brInst = dyn_cast<BranchInst>(loopI))
                {
                    if (brInst->isUnconditional())
                    {
                        BasicBlock *succ = brInst->getSuccessor(0);
                        for (BasicBlock *Pred : predecessors(BB))
                        {
                            // Successor same as predecessor
                            if (Pred == succ)
                            {
                                for (BasicBlock::InstListType::iterator subIt =
                                         succ->getInstList().begin();
                                     subIt != succ->getInstList().end(); ++subIt)
                                {
                                    Instruction *subI = &*subIt;

                                    if (auto *brInst = dyn_cast<BranchInst>(subI))
                                    {
                                        if (!(brInst->isUnconditional()))
                                        {
                                            // Successor basic blocks (taken and not taken)
                                            BasicBlock *succ0 = brInst->getSuccessor(0);
                                            // BasicBlock *succ1 = brInst->getSuccessor(1);

                                            // Get previous cmp values
                                            CmpInst *cmpInst = mapCmp->find(brInst->getCondition())->second;
                                            ICmpInst::Predicate pred = cmpInst->getPredicate();
                                            Value *oper0 = cmpInst->getOperand(0);
                                            Value *oper1 = cmpInst->getOperand(1);

                                            // Default new range pairs and value
                                            // VAL1: Range of the cmp instruction for branch taken
                                            std::pair<int, int> rangeCmpTaken(infMin, infMax);
                                            std::pair<int, int> rangeCmpNotTaken(infMin, infMax);
                                            Value *oper = oper0;

                                            // a < b
                                            if (oper0->hasName() && oper1->hasName())
                                            {
                                                errs() << "\n\nUNEXPECTED DOUBLE REFERENCE CMP INSTRUCTION\n\n";
                                            }
                                            // a < 1
                                            else if (oper0->hasName())
                                            {
                                                // Select reference to operand0
                                                oper = oper0;

                                                if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                                {
                                                    // Change range of successors based on reference, constant value, and predicate
                                                    computeCmpRange(true, pred, oper, CI->getZExtValue(), &rangeCmpTaken, &rangeCmpNotTaken);
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
                                                    computeCmpRange(false, pred, oper, CI->getZExtValue(), &rangeCmpTaken, &rangeCmpNotTaken);
                                                }
                                            }

                                            // VAL3: Range in current basic block of the variable in the cmp instruction
                                            std::map<Value *, std::pair<int, int>>::iterator valRefSource = getValueReference(BB, oper, listRange, infMin, infMax);
                                            // VAL4: Range in taken basic block of the variable in the cmp instruction
                                            std::map<Value *, std::pair<int, int>>::iterator valBranchTaken = getValueReference(succ0, oper, listRange, infMin, infMax);

                                            // Final computed branch ranges
                                            std::pair<int, int> rangeBranchTaken = brOpe(valRefSource->second, valBranchTaken->second, rangeCmpTaken);
                                            if (oper != inst && rangeBranchTaken.first != infMin && rangeBranchTaken.second != infMax)
                                            {
                                                tripcount = std::abs(rangeBranchTaken.second - rangeBranchTaken.first) + 1;
                                            }
                                        }
                                    }
                                }

                                // Search add/sub instruction
                                if (tripcount != -1)
                                {
                                    errs() << "Tripcount " << tripcount << "\n";
                                    for (BasicBlock::InstListType::iterator subIt =
                                             BB->getInstList().begin();
                                         subIt != BB->getInstList().end(); ++subIt)
                                    {
                                        Instruction *subI = &*subIt;
                                        if (auto *operInst = dyn_cast<BinaryOperator>(subI))
                                        {
                                            // Add/Sub in which the result is the operand in the phi instruction
                                            if (operInst->getName() == operand->getName())
                                            {
                                                Value *oper0 = operInst->getOperand(0);
                                                Value *oper1 = operInst->getOperand(1);
                                                unsigned operCode = operInst->getOpcode();

                                                if (oper0->getName() == inst->getName())
                                                {
                                                    if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                                                    {
                                                        int constVal = CI->getZExtValue();
                                                        // a = a + 1 || a - (-1) -> Always growing
                                                        if ((operCode == Instruction::Add && constVal >= 0) || (operCode == Instruction::Sub && constVal <= 0))
                                                        {
                                                            errs() << "Sum " << baseVal << " on " << constVal << " for " << tripcount << "\n";
                                                            errs() << "=" << ((constVal * tripcount) + baseVal) << "\n";
                                                            tripPair->first = baseVal;
                                                            tripPair->second = ((constVal * tripcount) + baseVal);
                                                        }
                                                        // a = a + (-1) || a - 1 -> Always smaller
                                                        else if ((operCode == Instruction::Add && constVal <= 0) || (operCode == Instruction::Sub && constVal >= 0))
                                                        {
                                                            errs() << "Sum " << baseVal << " on " << constVal << " for " << tripcount << "\n";
                                                            errs() << "=" << ((-constVal * tripcount) + baseVal) << "\n";
                                                            tripPair->first = ((-constVal * tripcount) + baseVal);
                                                            tripPair->second = baseVal;
                                                        }
                                                    }
                                                }
                                                else if (oper1->getName() == inst->getName())
                                                {
                                                    if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                                                    {
                                                        int constVal = CI->getZExtValue();
                                                        // a = a + 1 || a - (-1) -> Always growing
                                                        if ((operCode == Instruction::Add && constVal >= 0) || (operCode == Instruction::Sub && constVal <= 0))
                                                        {
                                                            errs() << "Sum " << baseVal << " on " << constVal << " for " << tripcount << "\n";
                                                            errs() << "=" << ((constVal * tripcount) + baseVal) << "\n";
                                                            tripPair->first = baseVal;
                                                            tripPair->second = ((constVal * tripcount) + baseVal);
                                                        }
                                                        // a = a + (-1) || a - 1 -> Always smaller
                                                        else if ((operCode == Instruction::Add && constVal <= 0) || (operCode == Instruction::Sub && constVal >= 0))
                                                        {
                                                            errs() << "Sum " << baseVal << " on " << constVal << " for " << tripcount << "\n";
                                                            errs() << "=" << ((-constVal * tripcount) + baseVal) << "\n";
                                                            tripPair->first = ((-constVal * tripcount) + baseVal);
                                                            tripPair->second = baseVal;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 1: The value is found and always grows higher
        // 2: The value is found and always grows smaller
        int searchInBasicBlock(Value *inst, BasicBlock *BB, Value *operand)
        {
            bool isSum = false, isSub = false, isFound = false;

            for (BasicBlock::InstListType::iterator subIt =
                     BB->getInstList().begin();
                 subIt != BB->getInstList().end(); ++subIt)
            {
                Instruction *subI = &*subIt;
                if (auto *operInst = dyn_cast<BinaryOperator>(subI))
                {
                    if (operInst->getName() == operand->getName())
                    {
                        Value *oper0 = operInst->getOperand(0);
                        Value *oper1 = operInst->getOperand(1);
                        unsigned operCode = operInst->getOpcode();

                        if (oper0->getName() == inst->getName())
                        {
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(oper1))
                            {
                                int constVal = CI->getZExtValue();
                                // a = a + 1 || a - (-1) -> Always growing
                                if ((operCode == Instruction::Add && constVal >= 0) || (operCode == Instruction::Sub && constVal <= 0))
                                {
                                    isSum = true;
                                    isFound = true;
                                }
                                // a = a + (-1) || a - 1 -> Always smaller
                                else if ((operCode == Instruction::Add && constVal <= 0) || (operCode == Instruction::Sub && constVal >= 0))
                                {
                                    isSub = true;
                                    isFound = true;
                                }
                            }
                        }
                        else if (oper1->getName() == inst->getName())
                        {
                            if (ConstantInt *CI = dyn_cast<ConstantInt>(oper0))
                            {
                                int constVal = CI->getZExtValue();
                                // a = a + 1 || a - (-1) -> Always growing
                                if ((operCode == Instruction::Add && constVal >= 0) || (operCode == Instruction::Sub && constVal <= 0))
                                {
                                    isSum = true;
                                    isFound = true;
                                }
                                // a = a + (-1) || a - 1 -> Always smaller
                                else if ((operCode == Instruction::Add && constVal <= 0) || (operCode == Instruction::Sub && constVal >= 0))
                                {
                                    isSub = true;
                                    isFound = true;
                                }
                            }
                        }
                    }
                }
            }

            // Found, but only sum xor sub
            if (isFound && isSum && !isSub)
            {
                return 1;
            }
            else if (isFound && !isSum && isSub)
            {
                return 2;
            }

            return 0;
        }

        // If basic block not already visited and not already inside workList, insert it in workList
        void applySimpleBr(bool isUpdated, BasicBlock *BB, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, std::vector<BasicBlock *> *workList, std::map<Value *, std::pair<int, int>> emptyMap)
        {
            bool isVisited = isAlreadyVisited(BB, listRange);
            if (!isVisited)
            {
                listRange->insert(std::pair<BasicBlock *, std::map<Value *, std::pair<int, int>>>(BB, emptyMap));
            }

            bool isInWL = isInWorkList(BB, workList);
            if ((!isVisited || isUpdated) && !isInWL)
            {
                errs() << "+ " << BB->getName() << " (notVisited=" << !isVisited << ", isUpdated=" << isUpdated << ")\n";
                workList->push_back(BB);
            }
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
            return interOpe(rangeOriginal, unionOpe(rangeSource0, rangeSource1));
        }

        // Range combining operation for br-complex instructions
        std::pair<int, int> brOpe(std::pair<int, int> rangeSource, std::pair<int, int> rangeDestination, std::pair<int, int> rangeBranch)
        {
            return interOpe(rangeBranch, interOpe(rangeSource, rangeDestination));
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
        void updateValueReference(BasicBlock *BB, Value *operand, std::pair<int, int> pairRange, std::map<BasicBlock *, std::map<Value *, std::pair<int, int>>> *listRange, int infMin, int infMax)
        {
            if (hasValueReference(BB, operand, listRange))
            {
                listRange->find(BB)->second.find(operand)->second.first = pairRange.first;
                listRange->find(BB)->second.find(operand)->second.second = pairRange.second;
                errs() << "UPDATE: ";
            }
            else
            {
                // Insert reference
                std::pair<Value *, std::pair<int, int>> newPairRange(operand, pairRange);
                listRange->find(BB)->second.insert(newPairRange);
                errs() << "NEW: ";
            }

            // Update reference
            errs() << operand->getName() << printRange(pairRange, infMin, infMax) << " in " << BB->getName() << "\n";
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

        std::string printRange(std::pair<int, int> rangeVal, int infMin, int infMax)
        {
            std::string valString = "(";

            if (rangeVal.first <= rangeVal.second)
            {
                valString += rangeVal.first == infMin ? "-Inf" : std::to_string(rangeVal.first);
                valString += ", ";
                valString += rangeVal.second == infMax ? "+Inf" : std::to_string(rangeVal.second);
                valString += ")";
            }
            else
            {
                valString += rangeVal.second == infMax ? "+Inf" : std::to_string(rangeVal.second);
                valString += ", ";
                valString += rangeVal.first == infMin ? "-Inf" : std::to_string(rangeVal.first);
                valString += ")";
            }

            return valString;
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
