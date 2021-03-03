#include "llvm/Analysis/DynAA/UtilityFunctions.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;
using namespace daa;
using namespace std;

//Checks whether V1 dominates V2.
bool dynAADominates(Value *V1, Value *V2, DominatorTree *DT) {
	Instruction *I1 = dyn_cast<Instruction>(V1);
	Instruction *I2 = dyn_cast<Instruction>(V2);
	if(I2) {
		if(I1) {
			return DT->dominates(I1, I2);
		}
		else {
			return true;
		}
	}
	else {
		return false;
	}
}

namespace daa {

	//SLVAPass::SLVAPass() : FunctionPass(ID) {}
	SLVAPass::SLVAPass() {}
	SLVAPass::~SLVAPass() {}

	void SLVAPass::replaceUndefCall(Function *F) {
    	for (auto &BB : *F) {
		    for (auto &I : BB) {
		        auto *Instr = &I;
				auto CI = dyn_cast<CallInst>(Instr);
				if (CI) {
					for (unsigned i = 0; i < CI->getNumArgOperands(); i++) {
						Value *Arg = CI->getArgOperand(i);
						if (isa<UndefValue>(Arg)) {
							//errs() << "replacing undef arg\n";
							CI->setArgOperand(i, Constant::getNullValue(Arg->getType()));
						}
					}
				}
				auto Phi = dyn_cast<PHINode>(Instr);
				if (Phi) {
					for (unsigned i = 0, e = Phi->getNumIncomingValues(); i != e; ++i) {
    					Value *Incoming = Phi->getIncomingValue(i);
    					if (isa<UndefValue>(Incoming)) {
							Phi->setIncomingValue(i, Constant::getNullValue(Phi->getType()));
						}
  					}
	  			}
			}
		}
	}

	void SLVAPass::getDebugInfo1(Function *DebugInfo, 
		GlobalVariable *TGV, DenseMap<const Value*, unsigned> ValueIDMapping, 
		unsigned FID, Function *F, DenseSet<Value*> PtrOperands, 
		DenseSet<Value*> Bases) {
		IntegerType *CharType = Type::getInt8Ty(F->getParent()->getContext());
  		PointerType *CharStarType = PointerType::getUnqual(CharType);
  		IntegerType *IntType = Type::getInt32Ty(F->getParent()->getContext());

	  	Instruction *Entry = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
	  	BitCastInst *BCI = new BitCastInst(TGV, CharStarType, "", Entry);
	
		errs() << "Printing Debug Info for " << FID << " " << F->getName() << "\n";
		DenseSet<Value *> :: iterator itr1;
		for (itr1 = Bases.begin(); itr1 != Bases.end(); ++itr1) {
		    Value *B1 = *itr1;
		    std::vector<Value*> Args;
			errs() << "Base: " << *B1 << " ID " << getValueID(B1, ValueIDMapping) << " \n";
		    Args.push_back(BCI);
			Args.push_back(ConstantInt::get(IntType, FID));
			Args.push_back(ConstantInt::get(IntType, getValueID(B1, ValueIDMapping)));
			if(isa<Argument>(B1)) {
				Args.push_back(new BitCastInst(B1, CharStarType, "", Entry));
				CallInst::Create(DebugInfo, Args, "", Entry);
			}
			else {
				Instruction *I = dyn_cast<Instruction>(B1);
				assert(I);
				Instruction *Loc;
				if (isa<PHINode>(I)) {
			    	Loc = I->getParent()->getFirstNonPHI();
			  	}
				else if (isa<InvokeInst>(I)) {
					return;
				}
			  	else {
			    	Loc = I->getNextNode();
			  	}
			  	Args.push_back(new BitCastInst(B1, CharStarType, "", Loc));
				CallInst::Create(DebugInfo, Args, "", Loc);
			}
		}
		
		DenseSet<Value *> :: iterator itr2;
		for (itr2 = PtrOperands.begin(); itr2 != PtrOperands.end(); ++itr2) {
			Value *O1 = *itr2;
			std::vector<Value*> Args;
			errs() << "Operand: " << *O1 << " ID " << getValueID(O1, ValueIDMapping) << " \n";
		    Args.push_back(BCI);
			Args.push_back(ConstantInt::get(IntType, FID));
			Args.push_back(ConstantInt::get(IntType, getValueID(O1, ValueIDMapping)));
			if(isa<Argument>(O1)) {
				Args.push_back(new BitCastInst(O1, CharStarType, "", Entry));
				CallInst::Create(DebugInfo, Args, "", Entry);
			}
			else {
				Instruction *I = dyn_cast<Instruction>(O1);
				assert(I);
				Instruction *Loc;
				if (isa<PHINode>(I)) {
			    	Loc = I->getParent()->getFirstNonPHI();
			  	}
				else if (isa<InvokeInst>(I)) {
					return;
				}
			  	else {
			    	Loc = I->getNextNode();
			  	}
			  	Args.push_back(new BitCastInst(O1, CharStarType, "", Loc));
				CallInst::Create(DebugInfo, Args, "", Loc);
			}
		}
	}

	bool LiveAnalysis::computeInSet(DenseSet<Value*> &Res, BasicBlock *BB, DenseMap<BasicBlock*, int> Visited, bool &Changed) {
		bool FirstIter = true;
		bool retry = true;
		for (BasicBlock *Pred : predecessors(BB)) {
			if (!Visited.count(Pred)) {
				Changed = true;
				continue;
			}
			retry = false;
			auto Term = Pred->getTerminator();
			if (FirstIter) {
				for (auto V : Out[Term]) {
					Res.insert(V);
				}
				FirstIter = false;
			}
			else {
				for (auto V : Res) {
					if (!Out[Term].count(V)) {
						Res.erase(V);
					}
				}
			}
		}
		return retry;
	}

	void LiveAnalysis::printBBInOut(Function &F) {
		for (auto &B : F) {
			BasicBlock *BB = &B;
			auto First = &BB->front();
			auto Last = BB->getTerminator();
			errs() << "BB:" << "\n";
			errs() << "first: " << *First << "\n";
			for (Value *V : In[First]) {
				errs() << *V << "\n";
			}
			errs() << "last: " << *Last << "\n";
			for (Value *V : Out[Last]) {
				errs() << *V << "\n";
			}
			errs() << "BB done\n";
		}
	}


//#if 0
	bool LiveAnalysis::run(Function &F) {
		bool Changed;
		DenseMap<BasicBlock*, int> Visited;
		SLVAPass SLVA;

		auto &EntryBB = F.getEntryBlock();
		auto &Inst = EntryBB.front();

		for (auto &B : F)
		{
			for (auto &I : B) {
				if (isa<InvokeInst>(&I)) {
					errs() << "Invoke in: " << F.getName() << "Live analysis\n";
					//assert(0 && "invoke in reaching def");
					return true;
				}
			}
		}
	
  		for (Function::arg_iterator AI = F.arg_begin(); AI != F.arg_end(); ++AI) {
    		if (SLVA.canTrackAliasInArg(AI)) {
					In[&Inst].insert(AI);
    		}
  		}

			int iter = 0;
			bool retry;


		do {
			iter++;
			if (iter > 90) {
				errs() << "ITER: " << iter << "\n";
				printBBInOut(F);
			}
			Changed = false;
			for (auto &B : F)
			{
				BasicBlock *BB = &B;
				Instruction *Prev = NULL;
				auto First = &BB->front();

				if (BB != &EntryBB) {
					DenseSet<Value*> Res;

				if (BB->hasNPredecessors(0) || (BB->getSinglePredecessor() == BB)) {
                    Visited[BB] = 1;
                    continue;
                }
                else {
                    //If the basic block has predecessors.
                    retry = computeInSet(Res, BB, Visited, Changed);
                    if (retry) {
                        continue;
                    }
                }

				for (Value *V : In[First]) {
					if (!Res.count(V)) {
						In[First].erase(V);
						Changed = true;
					}
				}

				for (Value *V : Res) {
					if (!In[First].count(V)) {
						In[First].insert(V);
						Changed = true;
					}
				}


			}

				for (auto &I : B)
				{
					Instruction *Instr = &I;
					//auto First = &BB->front();

					if (Prev) {
						In[Instr].clear();
						for (Value *V : Out[Prev]) {
							In[Instr].insert(V);
						}
					}

					Prev = Instr;

					Out[Instr].clear();
					for (auto V : In[Instr]) {
						Out[Instr].insert(V);
					}
				
					if(SLVA.canTrackAliasInInst(Instr, false)) {
							if(isa<PHINode>(Instr)) {
								//errs()<<*Instr<<"\n";
								for(unsigned i = 0; i < Instr->getNumOperands(); i++) {
		        					if(Out[Instr].count(Instr->getOperand(i))) {
		        						Out[Instr].erase(Instr->getOperand(i));
		        						//errs()<<*Instr<<" "<<*Instr->getOperand(i)<<"\n";
		        					}
		        				}
							}
							Out[Instr].insert(Instr);
        	}
      	}
				Visited[BB] = 1;
			}
		} while (Changed && iter < 100);

//#if 0
	if (iter == 100) {
		errs() << "Printing Function\n";
		errs() << F << "\n";

		for (auto &BB : F) {
			for (auto &I : BB) {
				errs() << "INSN: " << I << "\n";
				auto Inset = In[&I];
				auto Outset = Out[&I];
				errs() << "Printing In" << "\n";
				for (auto V : Inset) {
					errs() << *V << "\n";
				}
				errs() << "Printing Out" << "\n";
				for (auto V : Outset) {
					errs() << *V << "\n";
				}
				errs() << "\n";

			}
		}
		assert(0 && "reaching-def takes a lot of time");
	}
		
//#endif

		return false;
	}
//#endif


// Original live variable analysis
#if 0
	bool LiveAnalysis::run(Function &F) {

		bool Changed;
		DominatorTree DT(F);
		SLVAPass SLVA;

		do
		{
			Changed = false;
			auto &BBList = F.getBasicBlockList();
			for (auto BI = BBList.rbegin(); BI != BBList.rend(); BI++)
			{
				BasicBlock *BB = &*BI;
				auto Term = BB->getTerminator();

				for ( BasicBlock *Succ : successors(BB))
				{
					for (Value *V : In[&(*(Succ->begin()))])
					{
						if (!Out[Term].count(V)) {
							Out[Term].insert(V);
							Changed = true;
						}
					}
				}

				for (auto I = BB->rbegin(); I != BB->rend(); I++)
				{
					Instruction *Instr = &(*I);
					LiveVar gen, kill;

					if (Instr != Term) {
						auto NextInstr= Instr->getNextNode();
						for (auto V : In[NextInstr]) {
							if (!Out[Instr].count(V)) {
								Out[Instr].insert(V);
								Changed = true;
							}
						}
					}

					for (auto V : Out[Instr]) {
						In[Instr].insert(V);
					}

					if (SLVA.canTrackAliasInInst(Instr, false)) {
						kill.insert(Instr);
					}

					for (unsigned i = 0 ; i < Instr->getNumOperands(); i++) {
						auto Op = Instr->getOperand(i);
						Instruction *OI = dyn_cast<Instruction>(Op);
						Argument *OA = dyn_cast<Argument>(Op);
						if(OI && SLVA.canTrackAliasInInst(OI, false) && DT.dominates(OI, Instr)) {
							gen.insert(Op);
						}
						else if(OA && SLVA.canTrackAliasInArg(OA)) {
							gen.insert(Op);
						}
					}

					if (!kill.empty()) {
						for (auto V : kill) {
							In[Instr].erase(V);
						}
					}

					if (!gen.empty()) {
						for (auto V : gen) {
							In[Instr].insert(V);
						}
					}
					kill.clear();
					gen.clear();
      	}
			}
		}
		while (Changed);

#if 0
		errs() << F << "\n";
		for (auto &BB : F) {
			for (auto &I : BB) {
				errs() << "INSN: " << I << "\n";
				auto Inset = In[&I];
				auto Outset = Out[&I];
				errs() << "Printing In" << "\n";
				for (auto V : Inset) {
					errs() << *V << "\n";
				}
				errs() << "Printing Out" << "\n";
				for (auto V : Outset) {
					errs() << *V << "\n";
				}
				errs() << "\n";

			}
		}
#endif

		return false;
	}
#endif

	bool LiveAnalysis::isLiveBefore( Instruction *I, Value *V) {
		return In[I].count(V) != 0;
	}

	bool LiveAnalysis::isLiveAfter( Instruction *I, Value *V) {
		return Out[I].count(V) != 0;
	}
	void LiveAnalysis::pOut( Instruction *I) {
		errs() << "Out \n";
		for (auto I : Out[I]) {
			errs() << I->getName() << " ";
		}
		errs() << "\n";
	}
	void LiveAnalysis::pIn( Instruction *I) {
		errs() << "In \n";
		for (auto I : In[I]) {
			errs() << I->getName() << " ";
		}
		errs() << "\n";
	}
	void LiveAnalysis::print(LiveVar jset) {
		for (auto a : jset) {
			errs() << a->getName() << " ";
		}
		errs() << "\n";
	}

	void SLVAPass::run(Function &F) {
		LiveAnalysis L;
		L.run(F);
		InSet = L.returnInSet();
		OutSet = L.returnOutSet();
	}

	void SLVAPass::printBBInOut(Function &F) {
		for (auto &BB : F) {
			for (auto &II : BB) {
				auto inset = InSet[&II];
				errs() << "Insn: " << II << "\n";
				errs() << "IN:\n";
				for (auto V : inset) {
					errs() << *V << "\n";
				}
				auto outset = OutSet[&II];
				errs() << "Out:\n";
				for (auto V : outset) {
					errs() << *V << "\n";
				}
				errs() << "done.\n";
			}
		}
	}

	void SLVAPass::printValueIDMap(unsigned FID, Function *F, DenseMap<const Value *, unsigned> ValueIDMapping) {
		errs() << "Printing ValueIDMapping for " << FID << " " << F->getName() << "\n";
		DenseMap<const Value *, unsigned>::iterator it;
		for(it = ValueIDMapping.begin(); it != ValueIDMapping.end(); it++) {
			errs() << *it->first << " " << it->second << "\n";
		}
		errs() << "\n";
	}

	void SLVAPass::printIDValueMap(unsigned FID, Function *F, DenseMap<unsigned, Value *> IDValueMapping) {
		errs() << "Printing IDValueMapping for " << FID << " " << F->getName() << "\n";
		DenseMap<unsigned, Value *>::iterator it;
		for(it = IDValueMapping.begin(); it != IDValueMapping.end(); it++) {
			errs() << it->first << " " << *it->second << "\n";
		}
		errs() << "\n";
	}

	void SLVAPass::printFunctionIDMap(DenseMap<const Function *, unsigned> FunctionIDMapping) {
		errs() << "Printing FunctionIDMapping\n";
		DenseMap<const Function *, unsigned>::iterator it;
		for(it = FunctionIDMapping.begin(); it != FunctionIDMapping.end(); it++) {
			errs() << it->second << " " << *it->first << "\n";
		}
		errs() << "\n";
	}

	unsigned SLVAPass::assignUniqueID(Function *F, DenseMap<const Value *, unsigned> 
		&ValueIDMapping, DenseMap<unsigned, Value *> &IDValueMapping) {
		unsigned ValueID = 0;

	  	for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
			if (canTrackAliasInArg(AI)) {
				//errs() << ValueID << " " << *AI << "\n";
	      		addToValueMap(ValueID, AI, ValueIDMapping, IDValueMapping);
	        	ValueID++;
	    	}
	  	}

	  	for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
	    	for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
	      		Instruction *Instr = &*I;
				if (canTrackAliasInInst(Instr, false)) {
					//errs() << ValueID << " " << *Instr << "\n";
	          		addToValueMap(ValueID, Instr, ValueIDMapping, IDValueMapping);
	          		ValueID++;
				}
	    	}
	  	}
		return ValueID;
	}

	bool SLVAPass::addToValueMap(unsigned VID, Value *V, DenseMap<const Value *, unsigned> 
		&ValueIDMapping, DenseMap<unsigned, Value *> &IDValueMapping) {
		if (IDValueMapping.count(VID))
      		return true;

	    IDValueMapping[VID] = V;
	    ValueIDMapping[V] = VID;
	    return false;
	}

	unsigned SLVAPass::getValueID(const Value *V, DenseMap<const Value *, unsigned> ValueIDMapping) {
		if (ValueIDMapping.count(V))
    		return ValueIDMapping.lookup(V);
  		return InvalidID;
	}
	
	Value * SLVAPass::getIDValue(unsigned VID, DenseMap<unsigned, Value *> IDValueMapping) {
		return IDValueMapping.lookup(VID);
	}

	bool SLVAPass::addToFunctionMap(unsigned ID, const Function *F, DenseMap<const Function *, unsigned> &FunctionIDMapping) {
    	if (FunctionIDMapping.count(F))
      		return true;

    	FunctionIDMapping[F] = ID;
    	return false;
	}

	unsigned SLVAPass::getFunctionID(const Function *F, DenseMap<const Function *, unsigned> FunctionIDMapping) {
  		if (FunctionIDMapping.count(F))
      		return FunctionIDMapping.lookup(F);
    	return InvalidID;
	}

	bool SLVAPass::canTrackAliasInFunc(const Function *F) {
		if (F->isDeclaration())
		{
			return false;
		}

		for (auto &BB : *F)
		{
			for (auto &II : BB) {
				if (isa<InvokeInst>(&II)) {
					//errs() << "Invoke in: " << F->getName() << "\n";
					return false;
				}
			}
		}
		return true;
	}

	bool SLVAPass::canTrackAliasInArg(const Value *V) {
		return (V->getType()->isPointerTy() && !isa<Constant>(V));
	}


	bool SLVAPass::canTrackNoAliasInInst(const Instruction *I) {
		if (I->getType()->isPointerTy()) {
			if (isa<LoadInst>(I) || isa<CallInst>(I)) {
				return true;
			}
		}
		return false;
	}

	bool SLVAPass::canTrackAliasInInst(const Instruction *I, bool Selective) {
		if ((isa<IntrinsicInst>(I) && !isa<MemIntrinsic>(I)) || isa<Constant>(I)) {
	  	return false;
	  }

	  if (!Selective) {
	  	return I->getType()->isPointerTy();
	  }

		if (isa<LoadInst>(I) || isa<StoreInst>(I) || isa<AtomicRMWInst>(I) || isa<AtomicCmpXchgInst>(I) || isa<AnyMemIntrinsic>(I)) {
			return true;
		}

		return false;
	}

	void SLVAPass::readFunctionsFromFile(string Filename, std::map<string, unsigned> &NameToIDMap) {
		if (Filename.empty()) {
			return;
		}

  	string LogDir;
		const char *LogDirEnv;

  	/*LogDirEnv = getenv("LOG_DIR");

  	if (LogDirEnv) {
    		LogDir = LogDirEnv;
  	} 
  	else*/ {
    	errs() << "$LOG_DIR not set. Setting it to $HOME/logs\n";
			LogDirEnv = getenv("HOME");
			assert(LogDirEnv);
			LogDir = LogDirEnv;
			LogDir = LogDir + "/logs";
  	}

  	string DynamicAAFile = LogDir + "/" + Filename + ".txt";
  	//errs() << DynamicAAFile << "\n";
  	ifstream File(DynamicAAFile);
  	if (!File.is_open()) {
    	errs() << "Error in opening function name file.\n";
    	exit(0);
  	}

  	string str;
		int lineno = 0;
  	while (getline(File, str)) {
  		istringstream ss(str);
    	string FNAME;
			string DIS;
    	ss >> FNAME;
			ss >> DIS;
			unsigned disabled = (DIS.empty()) ? 0 : stoi(DIS);
			if (disabled != 1) {
    		NameToIDMap[FNAME] = lineno;
			}
			lineno++;
  	}
	}

	void SLVAPass::mapFunctionsToID(Module &M, string Filename, DenseMap<const Function *, unsigned> &FunctionIDMapping) {
		unsigned FID = 0;
		std::map<string, unsigned> NameIDMap;

		//errs() << "FILENAME: " << Filename << "\n";
		readFunctionsFromFile(Filename, NameIDMap);


		for (Module::iterator F = M.begin(); F != M.end(); ++F) {
			Function *Func = dyn_cast<Function>(F);
			
			if (!canTrackAliasInFunc(Func)) {
				continue;
			}

			if (!Filename.empty()) {
				if (!NameIDMap.count(Func->getName())) {
					continue;
				}
				FID = NameIDMap[Func->getName()];
			}
			//errs() << "Adding FID " << FID << " Func " << Func->getName() << " to map\n";
			FunctionIDMapping[Func] = FID;
			FID++;
		}
	}

	static bool isReachingDef(Function *F, Value *V1, Value *V2) {
		SLVAPass SLVA;
		SLVA.run(*F);
		LiveVar LV1, LV2;
		Instruction *I1 = dyn_cast<Instruction>(V1);

		if(!I1) {
			Instruction *I = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
			LV1 = SLVA.returnInSet(I);
		}
		else {
			LV1 = SLVA.returnInSet(I1);
		}

		Instruction *I2 = dyn_cast<Instruction>(V2);
		if(!I2) {
			Instruction *I = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
			LV2 = SLVA.returnInSet(I);
		}
		else {
			LV2 = SLVA.returnInSet(I2);
		}

		if(LV1.find(V2) == LV1.end() && LV2.find(V1) == LV2.end()) {
      		return false;
    	}

  		return true;
	}

	//AliasResult SLVAPass::checkAliasMetadata(Function *F, Value *V1, Value *V2) {
	AliasResult SLVAPass::checkAliasMetadata(Function *F, 
		const MemoryLocation &LocA, const MemoryLocation &LocB, 
		AAResults *AA, unsigned *MissedQueries) {
		//removeInvalidMetadata(F);
		Value *V1 = const_cast<Value *>(LocA.Ptr);
		Value *V2 = const_cast<Value *>(LocB.Ptr);
		if(isa<Constant>(V1) || isa<Constant>(V2)) {
			return MayAlias;
		}

  		//const DataLayout &DL = F->getParent()->getDataLayout();
		DominatorTree DT(*F);
		Instruction *I1 = dyn_cast<Instruction>(V1);
		Instruction *I2 = dyn_cast<Instruction>(V2);

		if(I1 && I2 && !DT.dominates(I1, I2) && !DT.dominates(I2, I1)) {
			return MayAlias;
		}

		Value *P1 = const_cast<Value *>(V1->stripPointerCastsAndInvariantGroups());
    	Value *P2 = const_cast<Value *>(V2->stripPointerCastsAndInvariantGroups());
    	AliasResult RemoveCastMetadataRes = MayAlias;
    	bool RemoveCastMetadataFound = false;
    	bool isPHIOpV1 = false, isPHIOpV2 = false;
		for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
	        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
	        	Instruction *Instr = &*I;
	        	if(isa<PHINode>(Instr)) {
                    for(unsigned i = 0; i < Instr->getNumOperands(); i++) {
                        if(Instr->getOperand(i) == V1) {
                            isPHIOpV1 = true;
                        }
                        if(Instr->getOperand(i) == V2) {
                            isPHIOpV2 = true;
                        }
                    }
                }

	          	CallInst *CI = dyn_cast<CallInst>(Instr);
	          	if (CI && CI->getIntrinsicID() == Intrinsic::noalias_dynaa) {
		            auto *MD1 = cast<MetadataAsValue>(CI->getArgOperand(0))->getMetadata();
		            auto *MD2 = cast<MetadataAsValue>(CI->getArgOperand(1))->getMetadata();
		            auto *MD3 = cast<MetadataAsValue>(CI->getArgOperand(2))->getMetadata();
		            auto *VV1 = dyn_cast<ValueAsMetadata>(MD1);
		            auto *VV2 = dyn_cast<ValueAsMetadata>(MD2);
		            auto *VV3 = dyn_cast<ValueAsMetadata>(MD3);
		            if (VV1 && VV2 && ((V1 == VV1->getValue() && V2 == VV2->getValue()) 
		            		       || (V2 == VV1->getValue() && V1 == VV2->getValue()))) {
									assert(VV3);
									auto V3 = dyn_cast<ConstantInt>(VV3->getValue());
									assert(V3);
									auto Size = V3->getZExtValue();
									if (Size == 0) {
										return MayAlias;
									}
									size_t Sz1 = 1, Sz2 = 1;
									if(LocA.Size.hasValue()) {
										Sz1 = LocA.Size.getValue();
									}
									else {
										return MayAlias;
									}
									//getSize(&DL, V1, &Sz1);
									assert(Sz1 != 0);
									if(LocB.Size.hasValue()) {
										Sz2 = LocB.Size.getValue();
									}
									else {
										return MayAlias;
									}
									//getSize(&DL, V2, &Sz2);
									assert(Sz2 != 0);

									size_t min = Sz1 < Sz2 ? Sz1 : Sz2;
									size_t max = Sz1 > Sz2 ? Sz1 : Sz2;
									if(Size >= min && Size < max) {
										++*MissedQueries;
									}

									if (Size < Sz1 || Size < Sz2) {
										return MayAlias;
									}
		            	return NoAlias;
		            }

		            //Remove pointer casts and check.
		            if (!RemoveCastMetadataFound && VV1 && VV2 && ((P1 == VV1->getValue() && P2 == VV2->getValue()) 
		            		       || (P2 == VV1->getValue() && P1 == VV2->getValue()))) {
									assert(VV3);
									auto V3 = dyn_cast<ConstantInt>(VV3->getValue());
									assert(V3);
									auto Size = V3->getZExtValue();
									if (Size == 0) {
										RemoveCastMetadataRes = MayAlias;
										RemoveCastMetadataFound = true;
									}
									size_t Sz1 = 1, Sz2 = 1;
									if(LocA.Size.hasValue()) {
										Sz1 = LocA.Size.getValue();
									}
									else {
										RemoveCastMetadataRes = MayAlias;
										RemoveCastMetadataFound = true;
									}
									//getSize(&DL, P1, &Sz1);
									assert(Sz1 != 0);
									if(LocB.Size.hasValue()) {
										Sz2 = LocB.Size.getValue();
									}
									else {
										RemoveCastMetadataRes = MayAlias;
										RemoveCastMetadataFound = true;
									}
									//getSize(&DL, P2, &Sz2);
									assert(Sz2 != 0);

									size_t min = Sz1 < Sz2 ? Sz1 : Sz2;
									size_t max = Sz1 > Sz2 ? Sz1 : Sz2;
									if(Size >= min && Size < max) {
										++*MissedQueries;
									}

									if (Size < Sz1 || Size < Sz2) {
										RemoveCastMetadataRes = MayAlias;
										RemoveCastMetadataFound = true;
									}
						if(!RemoveCastMetadataFound) {
							RemoveCastMetadataRes = NoAlias;
							RemoveCastMetadataFound = true;
						}
		            }
	          	}
	        }
		}

		if(RemoveCastMetadataFound) {
			return RemoveCastMetadataRes;
		}	

		for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
	        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
	        	Instruction *Instr = &*I;
	          	CallInst *CI = dyn_cast<CallInst>(Instr);
	          	if (CI && CI->getIntrinsicID() == Intrinsic::noalias_dynaa) {
		            auto *MD1 = cast<MetadataAsValue>(CI->getArgOperand(0))->getMetadata();
		            auto *MD2 = cast<MetadataAsValue>(CI->getArgOperand(1))->getMetadata();
		            auto *MD3 = cast<MetadataAsValue>(CI->getArgOperand(2))->getMetadata();
		            auto *VV1 = dyn_cast<ValueAsMetadata>(MD1);
		            auto *VV2 = dyn_cast<ValueAsMetadata>(MD2);
		            auto *VV3 = dyn_cast<ValueAsMetadata>(MD3);
		            if(VV1 && VV2 && (V1 == VV1->getValue() || V1 == VV2->getValue() || V2 == VV1->getValue() || V2 == VV2->getValue())) {
		            	AliasResult AR = MayAlias;
		            	if(V1 == VV1->getValue()) {
		            		//Since we need to chec for must alias only, size doesn't matter.
		            		const MemoryLocation &M1 = MemoryLocation(VV2->getValue(), LocationSize::unknown());
		            		AR = AA->alias(LocB, M1);
		            	}
		            	else if(V1 == VV2->getValue()) {
		            		//Since we need to chec for must alias only, size doesn't matter.
		            		const MemoryLocation &M1 = MemoryLocation(VV1->getValue(), LocationSize::unknown());
		            		AR = AA->alias(LocB, M1);
		            	}
		            	else if(V2 == VV1->getValue()) {
		            		//Since we need to chec for must alias only, size doesn't matter.
		            		const MemoryLocation &M1 = MemoryLocation(VV2->getValue(), LocationSize::unknown());
		            		AR = AA->alias(LocA, M1);
		            	}
		            	else if(V2 == VV2->getValue()) {
		            		//Since we need to chec for must alias only, size doesn't matter.
		            		const MemoryLocation &M1 = MemoryLocation(VV1->getValue(), LocationSize::unknown());
		            		AR = AA->alias(LocA, M1);
		            	}
		            	
						if (AR == MustAlias) {
							assert(VV3);
							auto V3 = dyn_cast<ConstantInt>(VV3->getValue());
							assert(V3);
							auto Size = V3->getZExtValue();
							if (Size == 0) {
								return MayAlias;
							}
							size_t Sz1 = 1, Sz2 = 1;
							if(LocA.Size.hasValue()) {
								Sz1 = LocA.Size.getValue();
							}
							else {
								return MayAlias;
							}
							//getSize(&DL, V1, &Sz1);
							assert(Sz1 != 0);
							if(LocB.Size.hasValue()) {
								Sz2 = LocB.Size.getValue();
							}
							else {
								return MayAlias;
							}
							//getSize(&DL, V2, &Sz2);
							assert(Sz2 != 0);

							size_t min = Sz1 < Sz2 ? Sz1 : Sz2;
							size_t max = Sz1 > Sz2 ? Sz1 : Sz2;
							if(Size >= min && Size < max) {
								++*MissedQueries;
							}
							if (Size < Sz1 || Size < Sz2) {
								return MayAlias;
							}
		            		return NoAlias;
						}
		            }     
				}
			}
		}

		Argument *A1 = dyn_cast<Argument>(V1);
		Argument *A2 = dyn_cast<Argument>(V2);

		Instruction *Entry = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
		if(I1 && A2) {
			MDNode* N = I1->getMetadata("AAProfInfo");
			int idx1 = A2->getArgNo();
			/*AttributeList AL = F->getAttributes();
			if(!N || !AL.hasAttribute(idx1, "AAProfInfo")) {
				errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
				return MayAlias;
			}*/
			MDNode* M = Entry->getMetadata("ArgDynAA");
			if(M) {
				bool found = false;
			    for(unsigned i = 0; i < M->getNumOperands(); i++) {
			    	if(stoi(cast<MDString>(M->getOperand(i))->getString()) == idx1) {
			    		found = true;
			    		break;
			    	}
			    }
			    if(!N || !found) {
			    	//errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
					return MayAlias;
			    }
			}
		}
		else if(A1 && I2) {
			int idx1 = A1->getArgNo();
			//AttributeList AL = F->getAttributes();
			MDNode* N = I2->getMetadata("AAProfInfo");
			/*if(!N || !AL.hasAttribute(idx1, "AAProfInfo")) {
				errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
				return MayAlias;
			}*/
			MDNode* M = Entry->getMetadata("ArgDynAA");
			if(M) {
				bool found = false;
			    for(unsigned i = 0; i < M->getNumOperands(); i++) {
			    	if(stoi(cast<MDString>(M->getOperand(i))->getString()) == idx1) {
			    		found = true;
			    		break;
			    	}
			    }
			    if(!N || !found) {
			    	//errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
					return MayAlias;
			    }
			}
		}
		else if(A1 && A2) {
			int idx1 = A1->getArgNo();
			//AttributeList AL1 = F->getAttributes();
			int idx2 = A2->getArgNo();
			//AttributeList AL2 = F->getAttributes();
			/*if(!!AL1.hasAttribute(idx1, "AAProfInfo") || !AL2.hasAttribute(idx2, "AAProfInfo")) {
				errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
				return MayAlias;
			}*/
			MDNode* M = Entry->getMetadata("ArgDynAA");
			if(M) {
				bool found1 = false;
				bool found2 = false;
			    for(unsigned i = 0; i < M->getNumOperands(); i++) {
			    	if(stoi(cast<MDString>(M->getOperand(i))->getString()) == idx1) {
			    		found1 = true;
			    	}
			    	if(stoi(cast<MDString>(M->getOperand(i))->getString()) == idx2) {
			    		found2 = true;
			    	}
			    	if(found1 && found2) {
			    		break;
			    	}
			    }
			    if(!found1 || !found2) {
			    	//errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
					return MayAlias;
			    }
			}
		}
		else if(I1 && I2) {
			MDNode* N1 = I1->getMetadata("AAProfInfo");
			MDNode* N2 = I2->getMetadata("AAProfInfo");
			if(!N1 || !N2) {
				//errs() << F->getName() << " No metadata found for atleast one of the pointer " << *V1 << "  " << *V2 << "\n";
				return MayAlias;
			}
		}

		if(LocA.Size == LocationSize::unknown() || LocB.Size == LocationSize::unknown()) {
			//errs() << F->getName() << " Unknown location size for " << *V1 << "  " << *V2 << "\n";
			return MayAlias;
		}

		/*if(!isReachingDef(F, V1, V2)) {
			if(isPHIOpV1 == true || isPHIOpV2 == true) {
                //errs() << F->getName() << " Pointers are not reaching due to PHI " << *V1 << "  " << *V2 << "\n";
                return MayAlias;
            }
			//errs() << F->getName() << " Pointers are not in the reaching definitions set " << *V1 << "  " << *V2 << "\n";
			return MayAlias;	
		}*/

		//errs() << F->getName() << " No information found for " << *V1 << "  " << *V2 << "\n";
		return MayAlias;
	}

	void SLVAPass::isArgorInst(Value *V, DenseMap<Instruction *, bool> &DefInst, DenseMap<Value *, bool> &Args, bool isOperand) {
		if(isa<Argument>(V)) {
			if(canTrackAliasInArg(V)) {
				if(Args.count(V)) {
					if(!Args[V]) {
						Args[V] = isOperand;
					}
				}
				else {
					Args[V] = isOperand;
				}
				//errs() << "Pointers " << *V << " " << Args[V] << "\n";
			}
	    }
	    else if(Instruction *I = dyn_cast<Instruction>(V)) {
	    	if(canTrackAliasInInst(I, false)) {
	    		if(DefInst.count(I)) {
	    			if(!DefInst[I]) {
	    				DefInst[I] = isOperand;
	    			}
				}
				else {
					DefInst[I] = isOperand;
				}
				//errs() << "Pointers " << *V << " " << DefInst[I] << "\n";
	    	}
	    }
	}

	Value* SLVAPass::fetchPointerOperand(Instruction *I) {
		Value *PtrOperand = NULL;
	  	//const DataLayout &DL = I->getModule()->getDataLayout();
	  	if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
	  	  PtrOperand = LI->getPointerOperand();
	  	}
	    else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
	  	  PtrOperand = SI->getPointerOperand();
	  	} 
	    else if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(I)) {
	  	  PtrOperand = RMW->getPointerOperand();
	  	} 
	    else if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(I)) {
	  	  PtrOperand = XCHG->getPointerOperand();
	  	}	
	  	/*else if (auto *MI = dyn_cast<AnyMemIntrinsic>(I)) {
	    	//memcpy/memmove/memset.
	    	PtrOperand = MI->getRawDest()->getType()->isPointerTy() ? MI->getRawDest() : MI->getDest();
	  	}*/
	    else {
			errs() << "Bad Instruction: " << *I << "\n";
			assert(0);
		}
		if (isa<Constant>(PtrOperand) || isa<AllocaInst>(PtrOperand)) {
			return NULL;
		}
		return PtrOperand;
	}

	Value* SLVAPass::fetchSourcePointerOperand(Instruction *I) {
		Value *PtrOperand = NULL;
		if (auto *MI = dyn_cast<AnyMemTransferInst>(I)) {
	    	//memcpy/memmove.
	    	PtrOperand = MI->getRawSource()->getType()->isPointerTy() ? MI->getRawSource() : MI->getSource();
	  	}
	  	if (isa<Constant>(PtrOperand) || isa<AllocaInst>(PtrOperand)) {
			return NULL;
		}
	  	return PtrOperand;
	}

	void SLVAPass::getPointersInDefInst(Instruction *I, bool SelectiveInst, DenseMap<Instruction *, bool> &DefInst, DenseMap<Value *, bool> &Args) {
		if(SelectiveInst) {
			const DataLayout &DL = I->getModule()->getDataLayout();
			const Value *CV = GetUnderlyingObject(I, DL);
	        Value* V = const_cast<Value *>(CV);
	        if(Instruction *BI = dyn_cast<Instruction>(V)) {
	        	//Instruction as base.
	        	if(isa<PHINode>(BI) && isa<SelectInst>(BI)) {
	        		//Multiple bases for PHI and Select instruction.
	        		SmallVector<const Value *, 10> Objects;
      				Function *F = I->getFunction();
      				DominatorTree DT(*F);
      				LoopInfo LI(DT);
      				GetUnderlyingObjects(V, Objects, DL, &LI, 0);
      				for(SmallVector<const Value *, 10>::iterator it = Objects.begin(); it != Objects.end(); ++it) {
					    isArgorInst(const_cast<Value *>(*it), DefInst, Args, false);
					}
	        	}
	        	else {
	        		isArgorInst(V, DefInst, Args, false);
	        	}
	        }
	        else {
	        	//Argument as base.
	        	isArgorInst(V, DefInst, Args, false);
	        }

            if(LoadInst *LI = dyn_cast<LoadInst>(I)) {
            	isArgorInst(LI->getPointerOperand(), DefInst, Args, true);
            }
            else if(StoreInst *SI = dyn_cast<StoreInst>(I)) {
            	isArgorInst(SI->getPointerOperand(), DefInst, Args, true);
            }
            else if(isa<CallInst>(I) && I->getType()->isPointerTy()) {
            	isArgorInst(I, DefInst, Args, true);
            }
        }
        else {
            DefInst[I] = true;
        }
	}

	void SLVAPass::getSize(const DataLayout* DL, Value *V, uint64_t *TP) {
		assert(V->getType()->isPointerTy());
		PointerType *PT = dyn_cast<PointerType>(V->getType());
      	Type *ElTy = PT->getElementType();
      	if(ElTy->isSized()) {
      		*TP = DL->getTypeAllocSize(ElTy);
        	return;
      	}
      	else {
      		*TP = 0;
      		return;
      	}
		/*if(PointerType *PT1 = dyn_cast<PointerType>(V->getType())) {
			if(PT1->getElementType()->isSized()) {
	          	*TP = DL->getTypeAllocSize(PT1->getElementType());
	          	return;
	        }
	        else if(PT1->getElementType()->isFunctionTy()) {
	        	FunctionType *FT = cast<FunctionType>(PT1->getElementType());
	    		*TP = DL->getTypeAllocSize(FT->getReturnType());
	          	return;
	        }
		}
		else {
	    	*TP = 0;
	    	assert(0 && "Value is not a PointerType.");
	  	}*/
	}

	AliasResult SLVAPass::getStaticAAResult(DataLayout* DL, Value *V1, Value *V2, DenseMap<Instruction *, bool> DefInst, DenseMap<Value *, bool> Args, AAResults &AA) {
		Instruction *I1 = dyn_cast<Instruction>(V1);
		Argument *A1 = dyn_cast<Argument>(V1);
		Instruction *I2 = dyn_cast<Instruction>(V2);
		Argument *A2 = dyn_cast<Argument>(V2);
		bool F1 = true;
		bool F2 = true;
		if(I1 && DefInst.count(I1)) {
			F1 = DefInst[I1];
		}
		else if(A1 && Args.count(A1)) {
			F1 = Args[A1];
		}

		if(I2 && DefInst.count(I2)) {
			F2 = DefInst[I2];
		}
		else if(A2 && Args.count(A2)) {
			F2 = Args[A2];
		}

		uint64_t TP1, TP2; 
    	getSize(DL, V1, &TP1);
    	getSize(DL, V2, &TP2);
    	auto L1 = LocationSize::unknown();
    	auto L2 = LocationSize::unknown();
    	auto L3 = LocationSize::unknown();
    	if(TP1 > 0) {
    		L1 = LocationSize::precise(TP1);
    	}

    	if(TP2 > 0) {
    		L2 = LocationSize::precise(TP2);
    	}

		if(F1 && F2) {
			const MemoryLocation &M1 = MemoryLocation(V1, L1);
			const MemoryLocation &M2 = MemoryLocation(V2, L2);
     		return AA.alias(M1, M2);
		}
		else if(F1 && TP1 > 0 && !F2) {
			const MemoryLocation &M1 = MemoryLocation(V1, L1);
			const MemoryLocation &M2 = MemoryLocation(V2, L3);
     		return AA.alias(M1, M2);
		}
		else if(!F1 && F2 && TP2) {
			const MemoryLocation &M1 = MemoryLocation(V1, L3);
			const MemoryLocation &M2 = MemoryLocation(V2, L2);
     		return AA.alias(M1, M2);
		}
		else {
			const MemoryLocation &M1 = MemoryLocation(V1, L3);
			const MemoryLocation &M2 = MemoryLocation(V2, L3);
     		return AA.alias(M1, M2);
		}	
	}

	void SLVAPass::getExtraOperands(Instruction *I, BasicBlock *StartBB, const PHITransAddr &Pointer, 
		  DenseSet<Value *> &TmpPtrOperands, DenseMap<BasicBlock *, Value *> &Visited, 
		  DominatorTree *DT, AssumptionCache &AC, DenseMap<Value *, uint64_t> &PtrSizeMap, uint64_t size) {
		assert(Pointer.getAddr());
		if(Pointer.IsPotentiallyPHITranslatable() && StartBB->hasNPredecessors(0)) {
            if(!PtrSizeMap.count(Pointer.getAddr()) && !isa<Constant>(Pointer.getAddr())) {
                TmpPtrOperands.insert(Pointer.getAddr());
                PtrSizeMap[Pointer.getAddr()] = size; 
            }
            return;
        }

		PredIteratorCache PredCache;
		vector<BasicBlock *> BlockList;
		BlockList.push_back(StartBB);
		while(!BlockList.empty()) {
		    BasicBlock *CurBB = BlockList.back();
 		    BlockList.pop_back();
		    vector<pair<BasicBlock *, PHITransAddr>> PredList;

		    for (BasicBlock *Pred : PredCache.get(CurBB)) {
		      	PredList.push_back(std::make_pair(Pred, Pointer));
		      	PHITransAddr &PredPointer = PredList.back().second;
		      	PredPointer.PHITranslateValue(CurBB, Pred, DT, false);
		      	Value *PredPtrVal = PredPointer.getAddr();
		      	pair<DenseMap<BasicBlock *, Value *>::iterator, bool> InsertRes =
		          	Visited.insert(std::make_pair(Pred, PredPtrVal));
		      	if (!InsertRes.second) {
		        	PredList.pop_back();
		      	}
		      	else {
		      		if(PredPtrVal) {
		      			BlockList.push_back(Pred);
		      		}
		      	}
		    }

		    if(PredList.size() == 0) {
                if(!PtrSizeMap.count(Pointer.getAddr()) && !isa<Constant>(Pointer.getAddr())) {
                    TmpPtrOperands.insert(Pointer.getAddr());
                    PtrSizeMap[Pointer.getAddr()] = size;
                }
            }


		    for (unsigned i = 0; i < PredList.size(); ++i) {
		      	BasicBlock *Pred = PredList[i].first;
		      	PHITransAddr &PredPointer = PredList[i].second;
		      	Value *PredPtrVal = PredPointer.getAddr();
		      	if(PredPtrVal) {
		       		getExtraOperands(I, Pred, PredPointer, TmpPtrOperands, Visited, DT, AC, PtrSizeMap, size);
		      	}
		      	else {
		      		if(!PtrSizeMap.count(Pointer.getAddr()) && !isa<Constant>(Pointer.getAddr())) {
		      			TmpPtrOperands.insert(Pointer.getAddr());
			        	PtrSizeMap[Pointer.getAddr()] = size;
		      		}
		      	}
		    }
		}
	}

	void SLVAPass::removeInvalidMetadata(Function *F) {
		clock_t CurrTime = clock();
    	double time_taken = ((double)(CurrTime - PrevTime))/CLOCKS_PER_SEC;
		if(time_taken < 0.0001) {
			return;
		}
		PrevTime = CurrTime;
		DenseSet<CallInst *> InvalidMetadataInst;
		for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
	        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
	        	Instruction *Instr = &*I;
	        	CallInst *CI = dyn_cast<CallInst>(Instr);
	          	if (CI && CI->getIntrinsicID() == Intrinsic::noalias_dynaa) {
	          		auto *MD1 = cast<MetadataAsValue>(CI->getArgOperand(0))->getMetadata();
		            auto *MD2 = cast<MetadataAsValue>(CI->getArgOperand(1))->getMetadata();
		            auto *VV1 = dyn_cast<ValueAsMetadata>(MD1);
		            auto *VV2 = dyn_cast<ValueAsMetadata>(MD2);
		            if(!VV1 || !VV2) {
		            	InvalidMetadataInst.insert(CI);
		            }
	          	}
	        }
	    }

	    for (auto Call : InvalidMetadataInst) {
	    	Call->eraseFromParent();
	    }
	}
}