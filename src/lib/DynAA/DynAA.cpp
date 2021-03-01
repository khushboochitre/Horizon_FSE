#include "llvm/Analysis/DynAA/DynAA.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"

using namespace std;
using namespace daa;
using namespace llvm;

static cl::opt<string> BenchName("bench-name");
static cl::opt<string> FunctionFile("prof-func-file");

static cl::opt<bool> SelectiveInst("prof-opt-selective", 
                        cl::init(false));
static cl::opt<bool> GetDebugInfo("prof-get-debug-info",
                        cl::init(false));

unsigned PointerPairs = 0;

DynAA::DynAA(): ModulePass(ID) {
	FileWrite = NULL;
	IntType = NULL;
  VoidType = NULL;
  DebugInfo = NULL;
}

bool DynAA::runOnModule(Module &M) {
 	setupScalarTypes(M);
	setUpHooks(M);

	unsigned MaxID = 0;

  //Debug information file name declared as global string.
  Constant* strConstant = ConstantDataArray::getString(M.getContext(), BenchName.c_str());
  GV = new GlobalVariable(M, strConstant->getType(), true,
            GlobalValue::InternalLinkage, strConstant, "");

  if(GetDebugInfo) {
    string TraceFile = BenchName + "_prof_trace";
    Constant* strConstant = ConstantDataArray::getString(M.getContext(), TraceFile.c_str());
    TGV = new GlobalVariable(M, strConstant->getType(), true,
                          GlobalValue::InternalLinkage, strConstant, "");
  }

  DenseMap<const Function *, unsigned> FunctionIDMapping;

  SLVAPass SLVA;

  //Assign IDs to functions on the basis of the list of function names present in the FunctionFile.
  SLVA.mapFunctionsToID(M, FunctionFile, FunctionIDMapping);

  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    SLVAPass SLVA;

    Function *Func = dyn_cast<Function>(F);
    if (F->isDeclaration()) {
      continue;
    }

		bool instrument = false;

		if (FunctionIDMapping.count(Func)) {
			instrument = true;
		}

    if(instrument) {
      SLVA.run(*Func);
      SLVA.replaceUndefCall(Func);
    }

    TargetLibraryInfo &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(*Func);

    //Stores the pointer and return instructions to instrument it.
    DenseMap<Instruction *, bool> DefInst;
    DenseSet<Instruction *> RetInst;
    DenseSet<Value*> PtrOperands;
    DenseSet<Value*> Bases;
    //DenseSet<Instruction *> UseInst;
    DenseMap<Value*, bool> Args;
    DenseMap<Value *, uint64_t> PtrSizeMap;

    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {

        Instruction *Instr = &*I;

        if (Func->getName().equals("main") && isa<ReturnInst>(I)) {
        	RetInst.insert(Instr);
        }

        if (CallInst *CI = dyn_cast<CallInst>(Instr)) {
       		Function *CalledFunc = CI->getCalledFunction();
        	if (CalledFunc && CalledFunc->getName().equals("exit")) {
        		RetInst.insert(Instr);
        	}
        }

        if(instrument) {
          if(CallInst *F = isFreeCall(Instr, &TLI)) {
            const DataLayout &DL = M.getDataLayout();
            Value *Obj = F->getOperand(0);
            if(!isa<Constant>(Obj)) {
              assert(Obj->getType()->isPointerTy());
              PtrOperands.insert(Obj);
              uint64_t Sz1 = 1;
              SLVA.getSize(&DL, Obj, &Sz1);
              PtrSizeMap[Obj] = Sz1;
            }
          }

          if(SLVA.canTrackAliasInInst(Instr, true)) {
            const DataLayout &DL = M.getDataLayout();
            if(auto *AI = dyn_cast<AnyMemIntrinsic>(Instr)) {
              MemoryLocation Loc = MemoryLocation::getForDest(AI);
              assert(Loc.Ptr);
              if(!isa<Constant>(Loc.Ptr)) {
                assert(Loc.Ptr->getType()->isPointerTy());
                PtrOperands.insert(const_cast<Value *>(Loc.Ptr));
                if(Loc.Size.hasValue()) {
                  PtrSizeMap[const_cast<Value *>(Loc.Ptr)] = Loc.Size.getValue();
                }
                else {
                  uint64_t Sz1 = 1;
                  SLVA.getSize(&DL, const_cast<Value *>(Loc.Ptr), &Sz1);
                  assert(Sz1 != 0);
                  PtrSizeMap[const_cast<Value *>(Loc.Ptr)] = Sz1;
                }
              }
            }
            else {
              Value *Ptr = SLVA.fetchPointerOperand(Instr);
              if (Ptr) {
                PtrOperands.insert(Ptr);
                MemoryLocation Loc = MemoryLocation::get(Instr);
                assert(Loc.Ptr);
                assert(Loc.Ptr->getType()->isPointerTy());
                assert(const_cast<Value *>(Loc.Ptr) == Ptr);
                if(Loc.Size.hasValue()) {
                  PtrSizeMap[Ptr] = Loc.Size.getValue();
                }
                else {
                  uint64_t Sz1 = 1;
                  SLVA.getSize(&DL, Ptr, &Sz1);
                  assert(Sz1 != 0);
                  PtrSizeMap[Ptr] = Sz1;
                }
              }
            }
        
            if(isa<AnyMemTransferInst>(Instr)) {
              const DataLayout &DL = M.getDataLayout();
              if(auto *AI = dyn_cast<AnyMemTransferInst>(Instr)) {
                MemoryLocation Loc = MemoryLocation::getForSource(AI);
                assert(Loc.Ptr);
                if(!isa<Constant>(Loc.Ptr)) {
                  assert(Loc.Ptr->getType()->isPointerTy());
                  PtrOperands.insert(const_cast<Value *>(Loc.Ptr));
                  if(Loc.Size.hasValue()) {
                    PtrSizeMap[const_cast<Value *>(Loc.Ptr)] = Loc.Size.getValue();
                  }
                  else {
                    uint64_t Sz1 = 1;
                    SLVA.getSize(&DL, const_cast<Value *>(Loc.Ptr), &Sz1);
                    assert(Sz1 != 0);
                    PtrSizeMap[const_cast<Value *>(Loc.Ptr)] = Sz1;
                  }
                }
              }
            }
            //SLVA.getPointersInDefInst(Instr, SelectiveInst, DefInst, Args); 
          }
        }
      }
    }

    DenseMap<const Value *, unsigned> ValueIDMapping;
    DenseMap<unsigned, Value *> IDValueMapping;

		if (instrument) {
    	//Assigns ID to every pointer instruction of a function.
    	MaxID = SLVA.assignUniqueID(Func, ValueIDMapping, IDValueMapping);
    	assert(ValueIDMapping.size() == IDValueMapping.size());
		}

    instrumentReturn(Func, RetInst);

		if (!instrument || PtrOperands.empty()) {
			continue;
		}

    DominatorTree DT(*Func);
    LoopInfo LI(DT);
    AssumptionCache &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*Func);

    DenseSet<Value *> TmpPtrOperands;
    for (auto Ptr : PtrOperands) {
      const DataLayout &DL = M.getDataLayout();
      DenseMap<BasicBlock *, Value *> Visited;
      if(Instruction *IO = dyn_cast<Instruction>(Ptr)) {
        PHITransAddr Address(Ptr, DL, &AC);
        if (Address.IsPotentiallyPHITranslatable()) {
          assert(PtrSizeMap.count(Ptr));
          SLVA.getExtraOperands(IO, IO->getParent(), Address, TmpPtrOperands, Visited, &DT, AC, PtrSizeMap, PtrSizeMap[Ptr]);
        }
      }
    }
    PtrOperands.insert(TmpPtrOperands.begin(), TmpPtrOperands.end());
    DenseSet<Value *> TmpPtrOperands1;
		for (auto Ptr : PtrOperands) {
	  	SmallVector<const Value *, 4> Objects;
			const DataLayout &DL = M.getDataLayout();
      GetUnderlyingObjects(Ptr, Objects, DL, &LI, 0);
			for (auto Obj : Objects) {
				if (!PtrOperands.count(Obj)) {
					if (!isa<Constant>(Obj) && !isa<AllocaInst>(Obj)) {
						//Bases.insert(const_cast<Value*>(Obj));
            TmpPtrOperands1.insert(const_cast<Value*>(Obj));
            uint64_t Sz1 = 1;
            SLVA.getSize(&DL, const_cast<Value*>(Obj), &Sz1);
            PtrSizeMap[const_cast<Value*>(Obj)] = Sz1;
					}
				}
			}
		}
    errs() << "Number of bases:: " << TmpPtrOperands1.size() << "\n";
    PtrOperands.insert(TmpPtrOperands1.begin(), TmpPtrOperands1.end());

    unsigned FID = SLVA.getFunctionID(Func, FunctionIDMapping);
    assert(FID != InvalidID);

    //SLVA.printValueIDMap(FID, Func, ValueIDMapping);

    Constant* funcConstant = ConstantDataArray::getString(M.getContext(), Func->getName());
    GlobalVariable* FGV = new GlobalVariable(M, funcConstant->getType(), true,
                                GlobalValue::InternalLinkage, funcConstant, "");
    FuncGlobalVarMap[FID] = FGV;

		int NumArgs = 0;
    for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
      if (SLVA.canTrackAliasInArg(AI)) {
				NumArgs++;
        //SLVA.isArgorInst(AI, DefInst, Args, false);
      }
    }

    //SLVA.run(*Func);

  	AAResults &AA = getAnalysis<AAResultsWrapperPass>(*F).getAAResults();
		Value *BasePtr = NULL;

		DenseSet<std::pair<Value*, Value*>> MayPairsOps;
		DenseSet<std::pair<Value*, Value*>> MayPairsBases;

    DenseSet<std::pair<unsigned, unsigned>> LivePairOperands;
    DenseSet<std::pair<unsigned, unsigned>> LivePairBases;

    getMayPairs(Func, SLVA, FID, PtrOperands, AA, true, MayPairsOps, ValueIDMapping, LivePairOperands, PtrSizeMap);
    getMayPairs(Func, SLVA, FID, Bases, AA, false, MayPairsBases, ValueIDMapping, LivePairBases, PtrSizeMap);

    /*DenseSet<pair<Value *, Value *>> OperandPointerPairs;
    getMorePointerPairs(Func, AA, OperandPointerPairs, &DT, AC, TLI);
    DenseSet<Value *> NewPtrOperands;
    DenseMap<Value *, Value *> FirstAliasMap;
    getNewPtrOperands(OperandPointerPairs, NewPtrOperands);
    getFirstAliasFor(Func, NewPtrOperands, FirstAliasMap, AA, &DT);
    DenseSet<pair<Value *, Value *>> NewPairOps;
    getFirstAliasPointerPairs(OperandPointerPairs, NewPairOps, FirstAliasMap, LivePairOperands, ValueIDMapping, &DT);*/

		for (auto MayPair : MayPairsOps) {
			auto Ptr = MayPair.first;
			auto MVar = MayPair.second;
			insertChecks1(Func, FID, MaxID, Ptr, MVar, BasePtr, SLVA, NumArgs, PtrOperands.size(), ValueIDMapping, true);
		}

		for (auto MayPair : MayPairsBases) {
			auto Ptr = MayPair.first;
			auto MVar = MayPair.second;
			insertChecks1(Func, FID, MaxID, Ptr, MVar, BasePtr, SLVA, NumArgs, PtrOperands.size(), ValueIDMapping, false);
		}

    /*for (auto MayPair : NewPairOps) {
      auto Ptr = MayPair.first;
      auto MVar = MayPair.second;
      if(!isa<Argument>(Ptr) && !isa<Argument>(MVar)) {
        assert(dynAADominates(MVar, Ptr, &DT));
      }
    
      errs() << Func->getName() << " PTR: " << *Ptr << " MVAR:  " << *MVar << "\n";
      insertChecks1(Func, FID, MaxID, Ptr, MVar, BasePtr, SLVA, NumArgs, PtrOperands.size(), ValueIDMapping, true);
    }*/

		//instrumentPointerArgs(Func, FID, MaxID, BasePtr, SLVA, Args, Args.size(), DefInst.size(), ValueIDMapping);
    //Adds instrumentation for Def.

    //instrumentPointerDef(Func, SLVA, FID, MaxID, DefInst, Args, BasePtr, Args.size(), DefInst.size(), ValueIDMapping);

    if(GetDebugInfo) {
      SLVA.getDebugInfo1(DebugInfo, TGV, ValueIDMapping, FID, Func, PtrOperands, Bases);
    }

    if (verifyFunction(*Func, &errs())) {
      errs() << "Not able to verify!\n";
      errs() << *Func << "\n";
      assert(0);
    }
  }

  errs() << "Number of instrumented pointer pairs:: " << PointerPairs << "\n";
	return false;
}

void DynAA::setUpHooks(Module &M) {
  vector<Type *> ArgTypes;
  ArgTypes.push_back(CharStarType);
  FunctionType *WriteFileType = FunctionType::get(VoidType, 
									ArgTypes, false);
  FileWrite = Function::Create(WriteFileType,
                               GlobalValue::ExternalLinkage,
                               "WriteToFile",
                               &M);

  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  FunctionType *FTy = FunctionType::get(CharStarType, ArgTypes, false);
  BasePtrHook = Function::Create(FTy,
                                 GlobalValue::ExternalLinkage,
                                 "getAABasePtr",
                                 &M);

  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  FunctionType *DTy = FunctionType::get(VoidType, ArgTypes, false);
  DebugInfo = Function::Create(DTy, GlobalValue::ExternalLinkage,
                               "getDebugInfo", &M);

  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  FunctionType *PTy = FunctionType::get(VoidType, ArgTypes, false);
  PrintValAddress = Function::Create(PTy, GlobalValue::ExternalLinkage,
                               "printValAddress", &M);

  ArgTypes.clear();
  PTy = FunctionType::get(Int64Ty, ArgTypes, false);
  GetAliasInfo = Function::Create(PTy, GlobalValue::ExternalLinkage,
                               "getAliasInfo", &M);
}

void DynAA::setupScalarTypes(Module &M) {
	VoidType = Type::getVoidTy(M.getContext());
  Int1Ty = Type::getInt1Ty(M.getContext());
	IntType = Type::getInt32Ty(M.getContext());
	Int64Ty = Type::getInt64Ty(M.getContext());
	CharType = Type::getInt8Ty(M.getContext());
  CharStarType = PointerType::getUnqual(CharType);
	Int64PtrTy = PointerType::getUnqual(Int64Ty);
  Int1PtrTy = PointerType::getUnqual(Int1Ty);
}

Value* DynAA::instrumentFuncEntry(Function *F, int FID, int MaxID, int ArgSize, int PointerSize)
{
  Instruction *Entry = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
	std::vector<Value*> Args;
	Args.push_back(ConstantInt::get(IntType, FID));
	Args.push_back(ConstantInt::get(IntType, MaxID));
  Args.push_back(new BitCastInst(FuncGlobalVarMap.lookup(FID), CharStarType, "", Entry));
  Args.push_back(ConstantInt::get(IntType, ArgSize));
  Args.push_back(ConstantInt::get(IntType, PointerSize));
	return CallInst::Create(BasePtrHook, Args, "", Entry);
}

struct AARecInfo {
	unsigned NotAlias;
	unsigned Alias;
};

static int getOffset(int Id1, int Id2, int MaxId)
{
	int tmp;
	assert(Id1 != Id2);
	if (Id1 > Id2) {
		tmp = Id1;
		Id1 = Id2;
		Id2 = tmp;
	}

	// MaxId + (MaxId - 1) + (MaxId  -2) + ...
	int XOff = 0;
	int i;
	for (i = 0; i < Id1; i++) {
		XOff += MaxId;
		MaxId--;
	}
	int YOff = Id2 - (Id1 + 1);
	int Off = XOff + YOff;

	return Off * sizeof(struct AARecInfo);
}

void DynAA::insertChecksLoc2(Function *F, unsigned FID, unsigned MaxID, 
	Value *Ptr, Value *MVar, Value* Base, 
  SLVAPass &SLVA, Instruction *Loc, 
	ValueIDMapTy ValueIDMapping, bool CheckPartial)
{
  unsigned PtrId = SLVA.getValueID(Ptr, ValueIDMapping);
  assert(PtrId != InvalidID);

  unsigned MayId = SLVA.getValueID(MVar, ValueIDMapping);
  assert(MayId != InvalidID);

  size_t NumSlots = ((size_t)MaxID * (MaxID + 1)) / 2;
  size_t AAInfoSz = NumSlots * sizeof(struct AARecInfo);
  int Off = getOffset(PtrId, MayId, MaxID);
  assert((size_t)Off <= AAInfoSz - sizeof(struct AARecInfo));

  IRBuilder<> IRB(Loc);

	auto CounterAddr = IRB.CreateGEP(CharType, Base, ConstantInt::get(IntType, Off));
  auto CounterAddr64 = IRB.CreateBitCast(CounterAddr, Int64PtrTy);
  auto CounterVal = IRB.CreateLoad(Int64Ty, CounterAddr64);

  auto P1 = IRB.CreateBitCast(Ptr, CharStarType);
  auto P2 = IRB.CreateBitCast(MVar, CharStarType);

  Value *Cmp = IRB.CreateICmp(ICmpInst::ICMP_SLT, P1, P2);
  Value *CmpEq = IRB.CreateICmp(ICmpInst::ICMP_EQ, P1, P2);
	auto P1Int = IRB.CreatePtrToInt(P1, Int64Ty);
	auto P2Int = IRB.CreatePtrToInt(P2, Int64Ty);
	auto Diff = IRB.CreateSelect(Cmp, IRB.CreateSub(P2Int, P1Int), IRB.CreateSub(P1Int, P2Int));
	auto EqMask = IRB.CreateShl(IRB.CreateZExt(CmpEq, Int64Ty), 48);
	auto AliasMask = IRB.CreateAnd(EqMask, CounterVal);

	Diff = IRB.CreateOr(Diff, AliasMask);

  auto Smaller = IRB.CreateICmp(ICmpInst::ICMP_SLT, Diff, CounterVal);
  Instruction *Br = SplitBlockAndInsertIfThen(Smaller, cast<Instruction>(Smaller)->getNextNode(), false);
  IRB.SetInsertPoint(Br);

  unsigned long long shift = (1ULL << 48);
  Value *CmpAlias = IRB.CreateICmp(ICmpInst::ICMP_EQ, CounterVal, ConstantInt::get(Int64Ty, shift));
  auto FinalDiff = IRB.CreateSelect(CmpAlias, ConstantInt::get(Int64Ty, 0), Diff);
  IRB.CreateStore(FinalDiff, CounterAddr64);
	/*Instruction *Br = SplitBlockAndInsertIfThen(Smaller, cast<Instruction>(Smaller)->getNextNode(), false);
	IRB.SetInsertPoint(Br);

  IRB.CreateStore(Diff, CounterAddr64);*/

  /*if (GetDebugInfo) {
    printValueAddress(FID, PtrId, MayId, Ptr, MVar, Loc, Sz1, Sz2);
  }*/
  PointerPairs++;
}

void DynAA::insertChecks1(Function *F, unsigned FID, unsigned MaxID,
		Value *Ptr, Value *MVar, Value* &BasePtr, 
  	SLVAPass &SLVA, int NumArgs, int NumPointers, 
		ValueIDMapTy ValueIDMapping, bool CheckPartial) {

	if (!BasePtr) {
		BasePtr = instrumentFuncEntry(F, FID, MaxID, NumArgs, NumPointers);
	}

  Instruction *Loc = NULL;
	Instruction *I = dyn_cast<Instruction>(Ptr);
	if (I) {
  	if (isa<PHINode>(I))
		{
    	Loc = I->getParent()->getFirstNonPHI();
  	}
		else if (isa<InvokeInst>(I))
		{
			auto InvInst = dyn_cast<InvokeInst>(I);
			auto UnwindDestBB = InvInst->getUnwindDest();
			assert(UnwindDestBB);
			if (UnwindDestBB->hasNPredecessors(1)) {
				auto UnwindDest = UnwindDestBB->getFirstNonPHI();
				assert(UnwindDest);
				insertChecksLoc2(F, FID, MaxID, Ptr, MVar, BasePtr, SLVA, UnwindDest, ValueIDMapping, CheckPartial);
			}
			auto NormalDestBB = InvInst->getNormalDest();
			assert(NormalDestBB);
			if (NormalDestBB->hasNPredecessors(1)) {
				Loc = NormalDestBB->getFirstNonPHI();
			}
			else {
				return;
			}
		}
  	else
		{
    	Loc = I->getNextNode();
  	}
	}
	else {
		assert(isa<Argument>(Ptr));
		Instruction *BasePtrI = dyn_cast<Instruction>(BasePtr);
		assert(BasePtrI);
		Loc = BasePtrI->getNextNode();
	}
	assert(Loc);
	insertChecksLoc2(F, FID, MaxID, Ptr, MVar, BasePtr, SLVA, Loc, ValueIDMapping, CheckPartial);
}


void DynAA::printValueAddress(unsigned FID, unsigned VID1, unsigned VID2, Value *Def, Value *MVar, Instruction *Loc, unsigned S1, unsigned S2) {
	//SLVAPass SLVA;
	//IRBuilder<> IRB(Loc);
    std::vector<Value*> Args;
	Args.push_back(ConstantInt::get(IntType, FID));
	Args.push_back(ConstantInt::get(IntType, VID1));
	Args.push_back(ConstantInt::get(IntType, VID2));
	Args.push_back(new BitCastInst(Def, CharStarType, "", Loc));
	Args.push_back(new BitCastInst(MVar, CharStarType, "", Loc));
  Args.push_back(ConstantInt::get(IntType, S1));
  Args.push_back(ConstantInt::get(IntType, S2));
	CallInst::Create(PrintValAddress, Args, "", Loc);
}


void DynAA::getMayPairs(Function *F, SLVAPass &SLVA,
	unsigned FID, DenseSet<Value *> Ptrs, AAResults &AA, 
	bool CheckPartial, DenseSet<std::pair<Value*, Value*>> &MayPairs,
	ValueIDMapTy ValueIDMapping, DenseSet<std::pair<unsigned, unsigned>> &LivePointerPairs,
  DenseMap<Value *, uint64_t> PtrSizeMap) {

	CheckPartial = true;
  //DataLayout* DL = new DataLayout(F->getParent());

  for (auto Ptr : Ptrs) {
		assert(isa<Instruction>(Ptr) || isa<Argument>(Ptr));
		Instruction *I = dyn_cast<Instruction>(Ptr);
		bool IsArgument = false;
		unsigned PtrID = SLVA.getValueID(Ptr, ValueIDMapping);
  	assert(PtrID != InvalidID);

    LiveVar LVars;

		if (!I) {
			I = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
			LVars = SLVA.returnInSet(I);
			IsArgument = true;
		}
		else {
			LVars = SLVA.returnOutSet(I);
		}
    for (auto LVar : LVars) {
      assert(LVar->getType()->isPointerTy());
      if (!Ptrs.count(LVar) || LVar == Ptr) {
        continue;
      }
			unsigned LVarID = SLVA.getValueID(LVar, ValueIDMapping);
  		assert(LVarID != InvalidID);
			auto Pair = (PtrID < LVarID) ? make_pair(PtrID, LVarID) : make_pair(LVarID, PtrID);
			if (!LivePointerPairs.count(Pair)) {
      	LivePointerPairs.insert(Pair);
			}
    }

    DenseSet<Value *> MayVars;
		bool Alias = false;

		uint64_t Sz1 = 1;
		if (CheckPartial) {
			//SLVA.getSize(DL, Ptr, &Sz1);
      assert(PtrSizeMap.count(Ptr));
      Sz1 = PtrSizeMap[Ptr];
			assert(Sz1 != 0);
		}

		const MemoryLocation &M1 = MemoryLocation(Ptr, LocationSize::precise(Sz1));

    for (auto LVar : LVars)
		{
			assert(LVar->getType()->isPointerTy());
			if (!Ptrs.count(LVar) || LVar == Ptr) {
				continue;
			}
			assert(!isa<Constant>(LVar));

			unsigned LVarID = SLVA.getValueID(LVar, ValueIDMapping);
  		assert(LVarID != InvalidID);

			if (IsArgument) {
				// ordering
				if (PtrID < LVarID) {
					continue;
				}
			}

			uint64_t Sz2 = 1;
			if (CheckPartial) {
				//SLVA.getSize(DL, LVar, &Sz2);
        assert(PtrSizeMap.count(LVar));
        Sz2 = PtrSizeMap[LVar];
				assert(Sz2 != 0);
			}
			const MemoryLocation &M2 = MemoryLocation(LVar, LocationSize::precise(Sz2));
      AliasResult AR = AA.alias(M1, M2);

			if (AR == MustAlias)
			{
				Alias = true;
				break;
			}
			else if (AR == MayAlias)
			{
				MayVars.insert(LVar);
				//errs() << "mayalias: " << FID << " " << PtrID << " " << LVarID << " Sz1: " << Sz1 << " Sz2: " << Sz2 << "\n";
				//errs() << "Ptr: " << *Ptr << " Lvar: " << *LVar << "\n";
			}
			else {
				//errs() << "noalias: " << FID << " " << PtrID << " " << LVarID << " Sz1: " << Sz1 << " Sz2: " << Sz2 << "\n";
				//errs() << "Ptr: " << *Ptr << " Lvar: " << *LVar << "\n";
			}
    }

		if (Alias) {
			continue;
		}

		for (auto MVar : MayVars) {
			MayPairs.insert(make_pair(Ptr, MVar));
		}
	}
}

void DynAA::instrumentReturn(Function *F, DenseSet<Instruction *> RetInst) {
  DenseSet<Instruction *> :: iterator itr;
  
  for (itr = RetInst.begin(); itr != RetInst.end(); ++itr) {
    //Instruction *I = *itr;
    std::vector<Value*> Args;
    Args.push_back(new BitCastInst(GV, CharStarType, "", *itr));
    CallInst::Create(FileWrite, Args, "", *itr);
  }
}

void DynAA::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AAResultsWrapperPass>();
  AU.addRequired<AssumptionCacheTracker>();
  //AU.addRequired<SLVAPass>();
}

char DynAA::ID = 0;

static RegisterPass<DynAA> X("dynaa",
                             "Dynamic Alias Analysis",
                             false, // Is CFG Only?
                             false); // Is Analysis?
