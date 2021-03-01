#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Analysis/DynAA/GetDynamicAAResults.h"

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iterator>
#include <iostream>
#include <iomanip>
#include <cstdio>

using namespace std;
using namespace llvm;
using namespace daa;

static cl::opt<string> BenchName("bench-name-read");
static cl::opt<string> FunctionFile("get-func-file");

static cl::opt<bool> SelectiveInst("get-opt-selective", 
                        cl::init(false));
static cl::opt<bool> GetDebugInfo("get-debug-info",
                       cl::init(false));

//static const unsigned InvalidID = -1;

GetDynamicAAResults::GetDynamicAAResults() : ModulePass(ID) {}

void GetDynamicAAResults::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<AAResultsWrapperPass>();
  AU.addRequired<AssumptionCacheTracker>();
}

void GetDynamicAAResults::setupScalarTypes(Module &M) {
  VoidType = Type::getVoidTy(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());
  Int64Ty = Type::getInt64Ty(M.getContext());
  CharType = Type::getInt8Ty(M.getContext());
  CharStarType = PointerType::getUnqual(CharType);
  Int64PtrTy = PointerType::getUnqual(Int64Ty);
}

void GetDynamicAAResults::addFuncDef(Module &M) {
  vector<Type *> ArgTypes;
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  FunctionType *FTy = FunctionType::get(VoidType, 
                      ArgTypes, false);
  CheckMustPairFunc = Function::Create(FTy,
                               GlobalValue::ExternalLinkage,
                               "isMustPair", &M);

  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(Int64Ty);
  ArgTypes.push_back(Int64Ty);
  FunctionType *NTy = FunctionType::get(VoidType, 
                      ArgTypes, false);
  CheckNoPairFunc = Function::Create(NTy,
                               GlobalValue::ExternalLinkage,
                               "isNoPair", &M);

  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  FunctionType *DTy = FunctionType::get(VoidType, ArgTypes, false);
  DebugInfo = Function::Create(DTy, GlobalValue::ExternalLinkage,
                               "getDebugInfo", &M);
}


void GetDynamicAAResults::loadDynamicAAResultsToMap(DenseMap<unsigned, 
  DenseMap<IntPair, LongIntPair>> &Aliases) {
  assert(!BenchName.empty());
  string LogFile = BenchName;

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

  string DynamicAAFile = LogDir + "/" + LogFile + ".txt";
  errs() << DynamicAAFile << "\n";
  ifstream File(DynamicAAFile);
  if(!File.good()) {
    errs() << "No profile information found! Filename:: " << DynamicAAFile << "\n";
    return;
  }

  if (!File.is_open()) {
    errs() << "Error in opening dynamicaa.txt\n";
    exit(0);
  }
  string str;
  while (getline(File, str)) {
    istringstream ss(str);
    string FID;
    string FNAME;
    string NumArgsStr;
    string NumPointersStr;
		string MaxID;
    ss >> FID;
    ss >> FNAME;
    ss >> NumArgsStr;
    ss >> NumPointersStr;
		ss >> MaxID;
    unsigned FuncID;
    FuncSignature FSign;
    FuncID = stoi(FID);
    FSign.NumArgs = stoi(NumArgsStr);
    FSign.NumPointers = stoi(NumPointersStr);
    FSign.Name = FNAME;
    FSign.MaxID = stoi(MaxID);
    FunctionSignaturesMap[FuncID] = FSign;
    string ID1, ID2;
    string Count1, Count2;
		//errs() << "reading records for " << FuncID << "\n";
    while (ss >> ID1 >> ID2 >> Count1 >> Count2) {
      IntPair ID;
      LongIntPair Count;
      ID.first = stoi(ID1);
      ID.second = stoi(ID2);
      Count.first = stoull(Count1);
      Count.second = stoull(Count2);
			assert(Count.second == 0);
			assert(ID.first < ID.second);
      Aliases[FuncID][ID] = Count;
			//errs() << ID1 << "(" << Count1 << ") " << ID2 << "(" << Count2 << ")\n";
			//errs() << "loading: " << FuncID << " " << ID.first << " " << ID.second << "\n";
    }
  }
  File.close();
}

void GetDynamicAAResults::verifyArgs(Function *F, DenseMap<unsigned, DenseMap<IntPair, LongIntPair>> &AllAliases, 
  unsigned FID, ValueIDMapTy ValueIDMapping, DenseMap<Value*, bool> &Args, DenseMap<Instruction *, bool> &DefInst) {
	SLVAPass SLVA;

  DenseMap<Value *, bool> :: iterator itr1;
  DenseMap<Value *, bool> :: iterator itr2;
  for (itr1 = Args.begin(); itr1 != Args.end(); ++itr1) {
    Value *Arg1 = itr1->first;
    unsigned Arg1Id = SLVA.getValueID(Arg1, ValueIDMapping);
    assert(Arg1Id != InvalidID);
  	for (itr2 = std::next(itr1); itr2 != Args.end(); ++itr2) {
    	Value *Arg2 = itr2->first;
  		unsigned Arg2Id = SLVA.getValueID(Arg2, ValueIDMapping);
      assert(Arg2Id != InvalidID);
	 		auto AliasPair1 = (Arg1Id < Arg2Id) ? make_pair(Arg1Id, Arg2Id) : make_pair(Arg2Id, Arg1Id);
			if (!AllAliases[FID].count(AliasPair1)) {
      	//AllAliases[FID][AliasPair1] = make_pair(0, 0);
				errs() << "No profile info for args: " << *Arg1 << "  and  " << *Arg2 << "\n";
     		assert(AllAliases[FID].count(AliasPair1));
			}
		}
	}
}

static int isDynMustAlias(unsigned long long truecount, unsigned long long falsecount) {
	assert(falsecount == 0);
	return truecount == (1ULL << 48);
}

static size_t getNoAliasSize(unsigned long long truecount, unsigned long long falsecount) {
	if (truecount > 0 && truecount != (1ULL << 48)) {
		return truecount;
	}
	return 0;
}

static int isDynMayAlias(unsigned long long truecount, unsigned long long falsecount) {
	assert(falsecount == 0);
	return truecount == 0;
}

static bool isStaticAlias(SLVAPass &SLVA, DenseMap<unsigned, Value*> &IDValueMapping, unsigned Id1, unsigned Id2, AAResults &AA, Value *Ptr) {
  Value *Ptr1 = SLVA.getIDValue(Id1, IDValueMapping);
  Value *Ptr2 = SLVA.getIDValue(Id2, IDValueMapping);
  const MemoryLocation &M1 = MemoryLocation(Ptr1, LocationSize::precise(1));
  const MemoryLocation &M2 = MemoryLocation(Ptr2, LocationSize::precise(1));
  AliasResult AR = AA.alias(M1, M2);
  if (AR != MustAlias) {
		/*errs() << "Two Candidates for replacement\n";
		errs() << "Cand1: " << *Ptr1 << "\n";
		errs() << "Cand2: " << *Ptr2 << "\n";
		errs() << "Ptr: " << *Ptr << "\n";*/
	}
	return true;
}

static void processPtr(Function *F, Value *Ptr, SLVAPass &SLVA, DenseMap<Value*, int> &Processed,
	DenseMap<unsigned, DenseMap<IntPair, LongIntPair>> &AllAliases, AAResults &AA, unsigned FID,
	DenseMap<const Value*, unsigned> ValueIDMapping, DenseSet<Value*> Ptrs, bool CheckPartial,
	DenseMap<int, int> &MustAliasMap, DenseMap<std::pair<int, int>, unsigned long long> &NoAliasMap,
	DenseSet<std::pair<int, int>> &MayAliasSet, DenseMap<unsigned, Value*> &IDValueMapping, 
  DenseSet<std::pair<unsigned, unsigned>> &LivePointerPairs, DenseSet<pair<unsigned, unsigned>> &AddedPairs,
  DenseMap<Value *, uint64_t> PtrSizeMap) {
	CheckPartial = true;
  //const DataLayout &DL = F->getParent()->getDataLayout();
  unsigned PtrId = SLVA.getValueID(Ptr, ValueIDMapping);
	bool IsArgument = isa<Argument>(Ptr);
  assert(PtrId != InvalidID);

	assert(isa<Instruction>(Ptr) || IsArgument);
	Instruction *I = dyn_cast<Instruction>(Ptr);
  LiveVar LVars;

	if (!I) {
		I = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
		LVars = SLVA.returnInSet(I);
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
    auto Pair = (PtrId < LVarID) ? make_pair(PtrId, LVarID) : make_pair(LVarID, PtrId);
    if (!LivePointerPairs.count(Pair)) {
      LivePointerPairs.insert(Pair);
    }
  }

  DenseSet<Value *> MayVars;
	Value *AliasVar = NULL;

	uint64_t Sz1 = 1;
	if (CheckPartial) {
		//SLVA.getSize(&DL, Ptr, &Sz1);
    assert(PtrSizeMap.count(Ptr));
    Sz1 = PtrSizeMap[Ptr];
		assert(Sz1 != 0);
	}

	//errs() << "Processing Ptr: " << PtrId << "\n";

	const MemoryLocation &M1 = MemoryLocation(Ptr, LocationSize::precise(Sz1));

  for (auto LVar : LVars)
	{
		assert(LVar->getType()->isPointerTy());
		if (!Ptrs.count(LVar) || Ptr == LVar) {
			continue;
		}
		assert(!isa<Constant>(LVar));
    unsigned LVarID = SLVA.getValueID(LVar, ValueIDMapping);
  	assert(LVarID != InvalidID);
		//errs() << "Processing LVAR: " << LVarID << " " << *LVar << "\n";

		// enforce ordering
		if (IsArgument && LVarID > PtrId) {
			continue;
		}

		if (!Processed[LVar]) {
			processPtr(F, LVar, SLVA, Processed, AllAliases, AA, FID, ValueIDMapping, Ptrs, CheckPartial, MustAliasMap, NoAliasMap, MayAliasSet, IDValueMapping, LivePointerPairs, AddedPairs, PtrSizeMap);
			assert(Processed[LVar]);
			//errs() << "Resuming Ptr: " << PtrId << "\n";
		}

		uint64_t Sz2 = 1;
		if (CheckPartial) {
			//SLVA.getSize(&DL, LVar, &Sz2);
      assert(PtrSizeMap.count(LVar));
      Sz2 = PtrSizeMap[LVar];
			assert(Sz2 != 0);
		}
		const MemoryLocation &M2 = MemoryLocation(LVar, LocationSize::precise(Sz2));
    AliasResult AR = AA.alias(M1, M2);

		if (AR == MustAlias)
		{
    	unsigned AID = SLVA.getValueID(LVar, ValueIDMapping);
    	assert(AID != InvalidID);

			int OldVal = (MustAliasMap.count(PtrId)) ? MustAliasMap[PtrId] : -1;
			// Only replace if static alias is dynamic alias with someone
			if (MustAliasMap.count(AID)) {
				MustAliasMap[PtrId] = MustAliasMap[AID];
        AddedPairs.insert(make_pair(PtrId, MustAliasMap[PtrId]));
				assert(OldVal == -1 || OldVal == MustAliasMap[PtrId]);
			}
			else {
				assert(OldVal == -1);
			}

    	AliasVar = LVar;
		}
		else if (AR == MayAlias)
		{
			//errs() << "MVAR: " << LVarID << " SZ: " << Sz2 << "\n";
			MayVars.insert(LVar);
		}
		else {
      auto AliasPair = PtrId < LVarID ? make_pair(PtrId, LVarID) : make_pair(LVarID, PtrId);
			size_t Sz = (Sz1 > Sz2) ? Sz1 : Sz2;
			if (AllAliases[FID].count(AliasPair)) {
				size_t S = getNoAliasSize(AllAliases[FID][AliasPair].first, AllAliases[FID][AliasPair].second);
				assert(S);
				if (Sz > S) {
					AllAliases[FID][AliasPair] = make_pair(Sz, 0);
				}
			}
			else {
				AllAliases[FID][AliasPair] = make_pair(Sz, 0);
			}
		}
  }

	if (AliasVar) {

    unsigned AID = SLVA.getValueID(AliasVar, ValueIDMapping);
    assert(AID != InvalidID);

  	for (auto MVar : MayVars) {
			unsigned VID = SLVA.getValueID(MVar, ValueIDMapping);
      assert(VID != InvalidID);

	 		auto AliasPair1 = AID < VID ? make_pair(AID, VID) : make_pair(VID, AID);
      auto AliasPair2 = PtrId < VID ? make_pair(PtrId, VID) : make_pair(VID, PtrId);
			if (!AllAliases[FID].count(AliasPair1)) {
				//errs() << "AID: " << AID << " VID: " << VID << " SZ1: " << Sz1 << " Name: " << F->getName() << "\n";
			}
    	assert(AllAliases[FID].count(AliasPair1));
			auto TrueCount = AllAliases[FID][AliasPair1].first;
			auto FalseCount = AllAliases[FID][AliasPair1].second;
			size_t Sz;

			if (isDynMayAlias(TrueCount, FalseCount)) {
				MayAliasSet.insert(make_pair(PtrId, VID));
        AddedPairs.insert(make_pair(PtrId, VID));
			}
			else if ((Sz = getNoAliasSize(TrueCount, FalseCount))) {
				NoAliasMap[make_pair(PtrId, VID)] = Sz;
        AddedPairs.insert(make_pair(PtrId, VID));
			}
			//errs() << "adding2: " << FID << " " << AliasPair2.first << " " << AliasPair2.second << "\n";
      AllAliases[FID][AliasPair2] = AllAliases[FID][AliasPair1];
		}
		Processed[Ptr] = 1;
		return;
  }
	
  for (auto MVar : MayVars) {
    unsigned VID = SLVA.getValueID(MVar, ValueIDMapping);
   	assert(VID != InvalidID);

    auto AliasPair1 = PtrId < VID ? make_pair(PtrId, VID) : make_pair(VID, PtrId);
		if (!AllAliases[FID].count(AliasPair1)) {
      AllAliases[FID][AliasPair1] = make_pair(0, 0);
		}
		auto TrueCount = AllAliases[FID][AliasPair1].first;
		auto FalseCount = AllAliases[FID][AliasPair1].second;
		size_t Sz;

		if (isDynMustAlias(TrueCount, FalseCount)) {
			int OldVal = (MustAliasMap.count(PtrId)) ? MustAliasMap[PtrId] : -1;
			MustAliasMap[PtrId] = (MustAliasMap.count(VID)) ? MustAliasMap[VID] : VID;
      AddedPairs.insert(make_pair(PtrId, MustAliasMap[PtrId]));
			if (OldVal != -1 && MustAliasMap[PtrId] != OldVal) {
        assert(isStaticAlias(SLVA, IDValueMapping, OldVal, MustAliasMap[PtrId], AA, Ptr));
      }
		}
		else if (isDynMayAlias(TrueCount, FalseCount)) {
			MayAliasSet.insert(make_pair(PtrId, VID));
      AddedPairs.insert(make_pair(PtrId, VID));
		}
		else if ((Sz = getNoAliasSize(TrueCount, FalseCount))) {
      NoAliasMap[make_pair(PtrId, VID)] = Sz;
      AddedPairs.insert(make_pair(PtrId, VID));
    }
	}
   
	Processed[Ptr] = 1;
}

static void generateMorePointerPairs(Function *F, 
  DominatorTree *DT, DenseMap<unsigned, Value*> IDValueMapping, 
  DenseMap<const Value*, unsigned> ValueIDMapping,
  DenseSet<pair<unsigned, unsigned>> AddedPairs, 
  DenseSet<std::pair<unsigned, unsigned>> LivePairOperands,
  DenseSet<std::pair<unsigned, unsigned>> LivePairBases, DenseSet<Value*> Bases, 
  DenseMap<int, int> &MustAliasMap,
  DenseMap<std::pair<int, int>, unsigned long long> &NoAliasSet,
  DenseSet<std::pair<int, int>> &MayAliasSet,
  DenseMap<Value *, uint64_t> &PtrSizeMap) {

  DataLayout* DL = new DataLayout(F->getParent());
  SLVAPass SLVA;

  DenseMap<unsigned, unsigned> TmpMustAliasMap;
  for(auto Pair : MustAliasMap) {
    Value *V1 = SLVA.getIDValue(Pair.first, IDValueMapping);
    Value *V2 = SLVA.getIDValue(Pair.second, IDValueMapping);

    Value *P1 = const_cast<Value *>(V1->stripPointerCastsAndInvariantGroups());
    Value *P2 = const_cast<Value *>(V2->stripPointerCastsAndInvariantGroups());
    if(P1 == P2 || isa<Constant>(P1) || isa<Constant>(P2)) {
      continue;
    }

    unsigned ID1 = SLVA.getValueID(P1, ValueIDMapping);
    assert(ID1 != InvalidID);
    unsigned ID2 = SLVA.getValueID(P2, ValueIDMapping);
    assert(ID2 != InvalidID);
    assert(ID1 != ID2);

    if(AddedPairs.count(make_pair(ID1, ID2)) || AddedPairs.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairOperands.count(make_pair(ID1, ID2)) || LivePairOperands.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairBases.count(make_pair(ID1, ID2)) || LivePairBases.count(make_pair(ID2, ID1))) {
      continue;
    }

    if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 < ID2) || dynAADominates(P1, P2, DT)) {
      if(!MustAliasMap.count(ID2)) {
        if(TmpMustAliasMap.count(ID2)) {
          Value *V3 = SLVA.getIDValue(TmpMustAliasMap[ID2], IDValueMapping);
          if(isa<Argument>(V3) && isa<Argument>(P1)) {
            TmpMustAliasMap[ID2] = ID1 < TmpMustAliasMap[ID2] ? ID1 : TmpMustAliasMap[ID2];
          }
          else {
            TmpMustAliasMap[ID2] = dynAADominates(P1, V3, DT) ? ID1 : TmpMustAliasMap[ID2];
          }
        }
        else {
          TmpMustAliasMap[ID2] = ID1;
        }
      }
    }
    else if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 > ID2) || dynAADominates(P2, P1, DT)) {
      if(!MustAliasMap.count(ID1)) {
        if(TmpMustAliasMap.count(ID1)) {
          Value *V3 = SLVA.getIDValue(TmpMustAliasMap[ID1], IDValueMapping);
          if(isa<Argument>(V3) && isa<Argument>(P2)) {
            TmpMustAliasMap[ID1] = ID2 < TmpMustAliasMap[ID1] ? ID2 : TmpMustAliasMap[ID1];
          }
          else {
            TmpMustAliasMap[ID1] = dynAADominates(P2, V3, DT) ? ID2 : TmpMustAliasMap[ID1];
          }
        }
        else {
          TmpMustAliasMap[ID1] = ID2;
        }
      }
    }
  }
  MustAliasMap.insert(TmpMustAliasMap.begin(), TmpMustAliasMap.end());

  DenseMap<std::pair<int, int>, unsigned long long> TmpNoAliasSet;
  DenseSet<std::pair<int, int>> TmpMayAliasSet;
  for(auto Pair : NoAliasSet) {
    Value *V1 = SLVA.getIDValue(Pair.first.first, IDValueMapping);
    Value *V2 = SLVA.getIDValue(Pair.first.second, IDValueMapping);

    Value *P1 = const_cast<Value *>(V1->stripPointerCastsAndInvariantGroups());
    Value *P2 = const_cast<Value *>(V2->stripPointerCastsAndInvariantGroups());
    if(P1 == P2 || isa<Constant>(P1) || isa<Constant>(P2)) {
      continue;
    }

    unsigned ID1 = SLVA.getValueID(P1, ValueIDMapping);
    assert(ID1 != InvalidID);
    unsigned ID2 = SLVA.getValueID(P2, ValueIDMapping);
    assert(ID2 != InvalidID);
    assert(ID1 != ID2);

    if(AddedPairs.count(make_pair(ID1, ID2)) || AddedPairs.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairOperands.count(make_pair(ID1, ID2)) || LivePairOperands.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairBases.count(make_pair(ID1, ID2)) || LivePairBases.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(TmpMayAliasSet.count(make_pair(ID1, ID2)) || TmpMayAliasSet.count(make_pair(ID2, ID1))) {
      continue;
    }

    uint64_t Sz1 = 1;
    //if (!Bases.count(P1)) {
    if(!PtrSizeMap.count(P1)) {
      SLVA.getSize(DL, P1, &Sz1);
      assert(Sz1 != 0);
      PtrSizeMap[P1] = Sz1;
    }
    else {
      Sz1 = PtrSizeMap[P1];
    }

    uint64_t Sz2 = 1;
    //if (!Bases.count(P2)) {
    if(!PtrSizeMap.count(P2)) {
      SLVA.getSize(DL, P2, &Sz2);
      assert(Sz2 != 0);
      PtrSizeMap[P2] = Sz2;
    }
    else {
      Sz2 = PtrSizeMap[P2];
    }

    uint64_t Sz3 = 1;
    //if (!Bases.count(V1)) {
      //SLVA.getSize(DL, V1, &Sz3);
      assert(PtrSizeMap.count(V1));
      Sz3 = PtrSizeMap[V1];
      assert(Sz3 != 0);
    //}

    uint64_t Sz4 = 1;
    //if (!Bases.count(V2)) {
      //SLVA.getSize(DL, V2, &Sz4);
      assert(PtrSizeMap.count(V2));
      Sz4 = PtrSizeMap[V2];
      assert(Sz4 != 0);
    //}

    if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 < ID2) || dynAADominates(P1, P2, DT)) {
      if(Sz1 == Sz3 && Sz2 == Sz4) {
        TmpNoAliasSet[make_pair(ID2, ID1)] = Pair.second;
      }
      else {
        TmpMayAliasSet.insert(make_pair(ID2, ID1));
        if(TmpNoAliasSet.count(make_pair(ID2, ID1))) {
          TmpNoAliasSet.erase(make_pair(ID2, ID1));
        }
      }
    }
    else if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 > ID2) || dynAADominates(P2, P1, DT)) {
      if(Sz1 == Sz3 && Sz2 == Sz4) {
        TmpNoAliasSet[make_pair(ID1, ID2)] = Pair.second;
      }
      else {
        TmpMayAliasSet.insert(make_pair(ID1, ID2));
        if(TmpNoAliasSet.count(make_pair(ID1, ID2))) {
          TmpNoAliasSet.erase(make_pair(ID1, ID2));
        }
      }
    }
  }
  NoAliasSet.insert(TmpNoAliasSet.begin(), TmpNoAliasSet.end());

  for(auto Pair : MayAliasSet) {
    Value *V1 = SLVA.getIDValue(Pair.first, IDValueMapping);
    Value *V2 = SLVA.getIDValue(Pair.second, IDValueMapping);

    Value *P1 = const_cast<Value *>(V1->stripPointerCastsAndInvariantGroups());
    Value *P2 = const_cast<Value *>(V2->stripPointerCastsAndInvariantGroups());
    if(P1 == P2 || isa<Constant>(P1) || isa<Constant>(P2)) {
      continue;
    }

    unsigned ID1 = SLVA.getValueID(P1, ValueIDMapping);
    assert(ID1 != InvalidID);
    unsigned ID2 = SLVA.getValueID(P2, ValueIDMapping);
    assert(ID2 != InvalidID);
    assert(ID1 != ID2);

    if(AddedPairs.count(make_pair(ID1, ID2)) || AddedPairs.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairOperands.count(make_pair(ID1, ID2)) || LivePairOperands.count(make_pair(ID2, ID1))) {
      continue;
    }

    if(LivePairBases.count(make_pair(ID1, ID2)) || LivePairBases.count(make_pair(ID2, ID1))) {
      continue;
    }

    if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 < ID2) || dynAADominates(P1, P2, DT)) {
      TmpMayAliasSet.insert(make_pair(ID2, ID1));
      if(NoAliasSet.count(make_pair(ID2, ID1))) {
        NoAliasSet.erase(make_pair(ID2, ID1));
      }
    }
    else if((isa<Argument>(P1) && isa<Argument>(P2) && ID1 > ID2) || dynAADominates(P2, P1, DT)) {
      TmpMayAliasSet.insert(make_pair(ID1, ID2));
      if(NoAliasSet.count(make_pair(ID1, ID2))) {
        NoAliasSet.erase(make_pair(ID1, ID2));
      }
    }
  }
  MayAliasSet.insert(TmpMayAliasSet.begin(), TmpMayAliasSet.end());
}

static void generateAllAliasesInfo1(unsigned FID, Function *F,
  DenseMap<unsigned, DenseMap<IntPair, LongIntPair>> &AllAliases, 
	SLVAPass &SLVA, AAResults &AA,
	DenseMap<const Value*, unsigned> &ValueIDMapping,
	DenseSet<Value*> Ptrs, bool CheckPartial,
	DenseMap<int, int> &MustAliasMap,
	DenseMap<std::pair<int, int>, unsigned long long> &NoAliasMap,
	DenseSet<std::pair<int, int>> &MayAliasSet, 
  DenseMap<unsigned, Value*> &IDValueMapping, 
  DenseSet<std::pair<unsigned, unsigned>> &LivePointerPairs,
  DenseSet<pair<unsigned, unsigned>> &AddedPairs,
  DenseMap<Value *, uint64_t> PtrSizeMap) {

	if (!AllAliases.count(FID)) {
		return;
	}

	//errs() << "generating info for " << F->getName() << "\n";
	DenseMap<Value*, int> Processed;

  for (auto Ptr : Ptrs) {
		Processed[Ptr] = 0;
	}

  for (auto Ptr : Ptrs) {
		if (!Processed[Ptr]) {
	 		processPtr(F, Ptr, SLVA, Processed, AllAliases, AA, FID, ValueIDMapping, Ptrs, CheckPartial, MustAliasMap, NoAliasMap, MayAliasSet, IDValueMapping, LivePointerPairs, AddedPairs, PtrSizeMap);
		}
  }
}

void GetDynamicAAResults::createNewDebugIR(Function *F, DenseMap<int, int> MustAliasMap, 
  DenseMap<std::pair<int, int>, unsigned long long> NoAliasMap,
  ValueIDMapTy ValueIDMapping, DenseMap<Value *, uint64_t> &PtrSizeMap) {
  SLVAPass SLVA;
  unsigned FID = SLVA.getFunctionID(F, FunctionIDMapping);
  assert(FID != InvalidID);
  //DataLayout* DL = new DataLayout(F->getParent());

  DenseMap<int, int>::iterator it;
  for (it = MustAliasMap.begin(); it != MustAliasMap.end(); it++)
  {
    Value *From = SLVA.getIDValue(it->first, IDValueMapping);
    Value *To = SLVA.getIDValue(it->second, IDValueMapping);

    Instruction *Inst;
    if(isa<Instruction>(From)) {
      Instruction *Loc = dyn_cast<Instruction>(From);
      if(isa<PHINode>(From)) {
        Inst = Loc->getParent()->getFirstNonPHI();
      }
      else {
        Inst = Loc->getNextNode();
      }
    }
    else {
      assert(isa<Argument>(From) && "neither inst nor arg");
      Inst = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
    }

    vector<Value *> Args;
    Args.push_back(ConstantInt::get(IntType, FID));
    Args.push_back(ConstantInt::get(IntType, it->first));
    Args.push_back(ConstantInt::get(IntType, it->second));
    Args.push_back(new BitCastInst(From, CharStarType, "", Inst));
    Args.push_back(new BitCastInst(To, CharStarType, "", Inst));
    CallInst::Create(CheckMustPairFunc, Args, "", Inst);
  }

  for (auto NoAliasPair : NoAliasMap) {
    Value *Ptr1 = SLVA.getIDValue(NoAliasPair.first.first, IDValueMapping);
    Value *Ptr2 = SLVA.getIDValue(NoAliasPair.first.second, IDValueMapping);
    size_t Size = NoAliasPair.second;
    uint64_t TP1, TP2; 
    //SLVA.getSize(DL, Ptr1, &TP1);
    //SLVA.getSize(DL, Ptr2, &TP2);
    assert(PtrSizeMap.count(Ptr1));
    assert(PtrSizeMap.count(Ptr2));
    TP1 = PtrSizeMap[Ptr1];
    TP2 = PtrSizeMap[Ptr2];
    assert(TP1 > 0 && TP2 > 0);
    //uint64_t Sz = TP1 > TP2 ? TP1 : TP2;
    if(Size < TP1 || Size < TP2) {
      continue;
    }
    vector<Value *> Args;
    Args.push_back(ConstantInt::get(IntType, FID));
    Args.push_back(ConstantInt::get(IntType, NoAliasPair.first.first));
    Args.push_back(ConstantInt::get(IntType, NoAliasPair.first.second));
    Instruction *InsertLoc = NULL;
    auto Inst = dyn_cast<Instruction>(Ptr1);
    if (Inst == NULL) {
      assert(isa<Argument>(Ptr1));
      InsertLoc = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
    }
    else {
      if (isa<PHINode>(Inst)) {
        InsertLoc = Inst->getParent()->getFirstNonPHI();
      }
      else {
        InsertLoc = Inst->getNextNode();
      }
    }
    assert(InsertLoc);
    Args.push_back(new BitCastInst(Ptr1, CharStarType, "", InsertLoc));
    Args.push_back(new BitCastInst(Ptr2, CharStarType, "", InsertLoc));
    Args.push_back(ConstantInt::get(Int64Ty, TP1));
    Args.push_back(ConstantInt::get(Int64Ty, TP2));
    CallInst::Create(CheckNoPairFunc, Args, "", InsertLoc);
  }
}

static void addAliasIntrinsic(Function *F, Value *Ptr1, Value *Ptr2, Value *InsertAfter, size_t Size)
{
	Instruction *InsertLoc = NULL;
	auto Inst = dyn_cast<Instruction>(InsertAfter);
	if (Inst == NULL) {
		assert(isa<Argument>(InsertAfter));
		InsertLoc = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
	}
	else {
  	if (isa<PHINode>(Inst)) {
			InsertLoc = Inst->getParent()->getFirstNonPHI();
		}
  	else {
  	  InsertLoc = Inst->getNextNode();
  	}
	}
	assert(InsertLoc);

  auto IntrinFn = Intrinsic::getDeclaration(F->getParent(), Intrinsic::noalias_dynaa);
	assert(IntrinFn);

	IRBuilder<> IRB(InsertLoc);
	std::vector<Value *> Args;
	Value *MV1 = MetadataAsValue::get(F->getContext(), ValueAsMetadata::get(Ptr1));
	Value *MV2 = MetadataAsValue::get(F->getContext(), ValueAsMetadata::get(Ptr2));
	Value *MV3 = MetadataAsValue::get(F->getContext(), ValueAsMetadata::get(ConstantInt::get(IRB.getInt64Ty(), Size)));
	Args.push_back(MV1);
	Args.push_back(MV2);
	Args.push_back(MV3);
	IRB.CreateCall(IntrinFn, Args);
}


static void insertNoAliasMetadata(Function *F, DenseMap<std::pair<int, int>, unsigned long long> &AliasMap, 
	DenseMap<unsigned, Value*> &IDValueMapping, DenseMap<std::pair<int, int>, unsigned long long> &NoAliasDuplicateCheckSet)
{
	SLVAPass SLVA;
	for (auto MapPair : AliasMap) {
		auto AliasPair = MapPair.first;
    std::pair<int, int> IDPair = AliasPair.first < AliasPair.second ? make_pair(AliasPair.first, AliasPair.second) : make_pair(AliasPair.second, AliasPair.first);
		if(NoAliasDuplicateCheckSet.count(IDPair)) {
      assert(MapPair.second == NoAliasDuplicateCheckSet[IDPair]);
    }
    else {
      Value *Ptr1 = SLVA.getIDValue(AliasPair.first, IDValueMapping);
      Value *Ptr2 = SLVA.getIDValue(AliasPair.second, IDValueMapping);
      size_t Size = MapPair.second;
      addAliasIntrinsic(F, Ptr1, Ptr2, Ptr1, Size);
      NoAliasDuplicateCheckSet[IDPair] = Size;
    }
	}
}

static void insertMayAliasMetadata(Function *F, DenseSet<std::pair<int, int>> &AliasSet, 
	DenseMap<unsigned, Value*> &IDValueMapping, 
  DenseMap<std::pair<int, int>, unsigned long long> &NoAliasDuplicateCheckSet,
  DenseSet<std::pair<int, int>> &MayAliasDuplicateCheckSet)
{
	SLVAPass SLVA;
	for (auto AliasPair : AliasSet) {
    std::pair<int, int> IDPair = AliasPair.first < AliasPair.second ? make_pair(AliasPair.first, AliasPair.second) : make_pair(AliasPair.second, AliasPair.first);
    assert(!NoAliasDuplicateCheckSet.count(IDPair));
    if(!MayAliasDuplicateCheckSet.count(IDPair)) {
      Value *Ptr1 = SLVA.getIDValue(AliasPair.first, IDValueMapping);
      Value *Ptr2 = SLVA.getIDValue(AliasPair.second, IDValueMapping);
      addAliasIntrinsic(F, Ptr1, Ptr2, Ptr1, 0);
      MayAliasDuplicateCheckSet.insert(IDPair);
    }
	}
}

static void replaceMustValues(Function *F, DenseMap<int, int> &MustAliasMap, DenseMap<unsigned, Value*> &IDValueMapping)
{
	SLVAPass SLVA;
  DenseMap<int, int>::iterator it;
  for (it = MustAliasMap.begin(); it != MustAliasMap.end(); it++)
	{
    Value *From = SLVA.getIDValue(it->first, IDValueMapping);
    Value *To = SLVA.getIDValue(it->second, IDValueMapping);

    if (From->getType() == To->getType()) {
      From->replaceAllUsesWith(To);
    }
    else {
      //Typecast to Def type and replace.
      Instruction *InsertLoc = dyn_cast<Instruction>(From);
			if (!InsertLoc) {
				assert(isa<Argument>(From) && "neither inst nor arg");
        InsertLoc = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
			}
			else {
        if (isa<PHINode>(InsertLoc)) {
          InsertLoc = InsertLoc->getParent()->getFirstNonPHI();
        }
      }
      IRBuilder<> IRB(InsertLoc);
      auto NewInst = IRB.CreateBitCast(To, From->getType());
      From->replaceAllUsesWith(NewInst);
    }
  }
}

bool GetDynamicAAResults::verifyFuncSignature(unsigned FID, Function *F, unsigned MaxID, int NumDefs, int NumArgs) {
  FuncSignature FSign = FunctionSignaturesMap[FID];
  if (!F->getName().equals(FSign.Name)) {
    errs() << "Function name " << FSign.Name << " " << F->getName() << "\n";
    return false;
  }

  if (FSign.NumArgs != (unsigned)NumArgs) {
    errs() << "Function arguments " << FSign.NumArgs << " " << NumArgs << "\n";
    return false;
  }

  if (FSign.NumPointers != (unsigned)NumDefs) {
    errs() << "Function pointers " << FSign.NumPointers << " " << NumDefs << "\n";
    return false;
  }

  if (FSign.MaxID != MaxID) {
    errs() << "Function pointer numbers " << FSign.MaxID << " " << MaxID << "\n";
    return false;
  }

  return true;
}

static void addMetadataForInstructions(Function *F, DenseSet<Value *> Bases, DenseSet<Value *> PtrOperands) {
  LLVMContext& C = F->getContext();
  Instruction *Entry = dyn_cast<Instruction>(F->begin()->getFirstInsertionPt());
  SmallVector<Metadata *, 32> Args;
  for(auto Ptr : Bases) {
    if(Argument *A = dyn_cast<Argument>(Ptr)) {
      //F->setAttributes(F->getAttributes().addAttribute(F->getContext(), A->getArgNo(), "AAProfInfo"));
      unsigned ArgNum = A->getArgNo();
      Args.push_back(llvm::MDString::get(C, to_string(ArgNum).c_str()));
    }
    else if(Instruction *I = dyn_cast<Instruction>(Ptr)) {
      MDNode *N = MDNode::get(I->getContext(), {});
      I->setMetadata("AAProfInfo", N);
    }
  }

  for(auto Ptr : PtrOperands) {
    if(Argument *A = dyn_cast<Argument>(Ptr)) {
      //F->setAttributes(F->getAttributes().addAttribute(F->getContext(), A->getArgNo(), "AAProfInfo"));
      unsigned ArgNum = A->getArgNo();
      Args.push_back(llvm::MDString::get(C, to_string(ArgNum).c_str()));
    }
    else if(Instruction *I = dyn_cast<Instruction>(Ptr)) {
      MDNode *N = MDNode::get(I->getContext(), {});
      I->setMetadata("AAProfInfo", N);
    }
  }

  if(Args.size() > 0) {
    auto *N =  MDTuple::get(C, Args);
    Entry->setMetadata("ArgDynAA", N);
  }
}

static void getAliasStatistics(DenseMap<unsigned, 
  DenseMap<IntPair, LongIntPair>> Aliases) {
  unsigned MustAlias = 0;
  unsigned MayAlias = 0;
  unsigned NoAlias = 0;
  DenseMap<unsigned, DenseMap<IntPair, LongIntPair>>::iterator itr1;
  for(itr1 = Aliases.begin(); itr1 != Aliases.end(); itr1++) {
    DenseMap<IntPair, LongIntPair>::iterator itr2;
    for(itr2 = itr1->second.begin(); itr2 != itr1->second.end(); itr2++) {
      auto TrueCount = itr2->second.first;
      auto FalseCount = itr2->second.second;

      if(isDynMustAlias(TrueCount, FalseCount)) {
        MustAlias++;
      }
      else if(isDynMayAlias(TrueCount, FalseCount)) {
        MayAlias++;
      }
      else if(size_t Sz = getNoAliasSize(TrueCount, FalseCount)) {
        NoAlias++;
      }
    }
  }
  errs() << "Printing statistics for profiling information\n";
  errs() << "Number of MustAlias pointer pairs:: " << MustAlias << "\n";
  errs() << "Number of MayAlias pointer pairs:: " << MayAlias << "\n";
  errs() << "Number of NoAlias pointer pairs:: " << NoAlias << "\n";
  errs() << "Printing done\n";
}

bool GetDynamicAAResults::runOnModule(Module &M) {
  //errs() << "GetDynamicAAResults called\n";
	//disableDynAA();
  DenseMap<unsigned, DenseMap<IntPair, LongIntPair>> Aliases;
  loadDynamicAAResultsToMap(Aliases);
  if(Aliases.size() < 1) {
    return false;
  }

  getAliasStatistics(Aliases);

  if(GetDebugInfo) {
    setupScalarTypes(M);
    addFuncDef(M);

    string TraceFile = BenchName + "_get_trace";
    Constant* strConstant = ConstantDataArray::getString(M.getContext(), TraceFile.c_str());
    TGV = new GlobalVariable(M, strConstant->getType(), true,
                          GlobalValue::InternalLinkage, strConstant, "");
  };
  
  SLVAPass SLVA;
  SLVA.mapFunctionsToID(M, FunctionFile, FunctionIDMapping);

  NoAliasIntrinsic = Intrinsic::getDeclaration(&M, Intrinsic::noalias_dynaa);
  assert(NoAliasIntrinsic);

  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    SLVAPass SLVA;

    Function *Func = dyn_cast<Function>(F);
    ValueIDMapTy ValueIDMapping;

		if (!Func->isDeclaration()) {
			SLVA.replaceUndefCall(Func);
		}

		if (!FunctionIDMapping.count(Func)) {
			continue;
		}

    TargetLibraryInfo &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(*Func);

    unsigned FID = SLVA.getFunctionID(Func, FunctionIDMapping);
    assert(FID != InvalidID);

    //DenseMap<Instruction *, bool> DefInst;
    DenseSet<Value*> PtrOperands;
    DenseMap<Value*, bool> Args;
    DenseSet<Value*> Bases;
    DenseMap<Value *, uint64_t> PtrSizeMap;

    if (Aliases.count(FID)) {
      SLVA.run(*Func);

      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
          Instruction *Instr = &*I;
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

          if (SLVA.canTrackAliasInInst(Instr, true)) {
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

			DominatorTree DT(*F);
			LoopInfo LI(DT);
      auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*Func);
      
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
      //errs() << "Number of bases:: " << TmpPtrOperands1.size() << "\n";
      PtrOperands.insert(TmpPtrOperands1.begin(), TmpPtrOperands1.end());

			int NumArgs = 0;
      for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
        if (SLVA.canTrackAliasInArg(AI)) {
          //SLVA.isArgorInst(AI, DefInst, Args, false);
					NumArgs++;
        }
      }

      //Convert Alias analysis information fetched in the form of ID to Values.
      SLVA.assignUniqueID(Func, ValueIDMapping, IDValueMapping);
      assert(ValueIDMapping.size() == IDValueMapping.size());

      bool match = verifyFuncSignature(FID, Func, ValueIDMapping.size(), PtrOperands.size(), NumArgs);
      if(!match) {
        errs() << "Function Signature invalid for " << Func->getName() << "\n";
        assert(match);
      }

			DenseMap<int, int> MustAliasMap;
			DenseMap<std::pair<int, int>, unsigned long long> NoAliasMap;
			DenseSet<std::pair<int, int>> MayAliasSet;

      DenseSet<std::pair<unsigned, unsigned>> LivePairOperands;
      DenseSet<std::pair<unsigned, unsigned>> LivePairBases;

  		AAResults &AA = getAnalysis<AAResultsWrapperPass>(*F).getAAResults();

      DenseSet<pair<unsigned, unsigned>> AddedPairs;
      generateAllAliasesInfo1(FID, Func, Aliases, SLVA, AA, ValueIDMapping, PtrOperands, true, MustAliasMap, NoAliasMap, MayAliasSet, IDValueMapping, LivePairOperands, AddedPairs, PtrSizeMap);
      generateAllAliasesInfo1(FID, Func, Aliases, SLVA, AA, ValueIDMapping, Bases, false, MustAliasMap, NoAliasMap, MayAliasSet, IDValueMapping, LivePairBases, AddedPairs, PtrSizeMap);
      generateMorePointerPairs(Func, &DT, IDValueMapping, ValueIDMapping, AddedPairs, LivePairOperands, LivePairBases, Bases, MustAliasMap, NoAliasMap, MayAliasSet, PtrSizeMap);

      if(GetDebugInfo) {
        SLVA.getDebugInfo1(DebugInfo, TGV, ValueIDMapping, FID, Func, PtrOperands, Bases);
        createNewDebugIR(Func, MustAliasMap, NoAliasMap, ValueIDMapping, PtrSizeMap);
      }
      else {
        replaceMustValues(Func, MustAliasMap, IDValueMapping);

        if (!NoAliasMap.empty() || !MayAliasSet.empty()) {
          DenseMap<std::pair<int, int>, unsigned long long> NoAliasDuplicateCheckSet;
          DenseSet<std::pair<int, int>> MayAliasDuplicateCheckSet;
          insertNoAliasMetadata(Func, NoAliasMap, IDValueMapping, NoAliasDuplicateCheckSet);
          insertMayAliasMetadata(Func, MayAliasSet, IDValueMapping, NoAliasDuplicateCheckSet, MayAliasDuplicateCheckSet);
          Func->addFnAttr("noalias_dynaa_intrinsic");
        } 
      }

      addMetadataForInstructions(Func, Bases, PtrOperands);

      if (verifyFunction(*Func, &errs())) {
        errs() << "Not able to verify!\n";
        errs() << *Func << "\n";
        assert(0);
      }
    }

    IDValueMapping.clear();
  }
	//enableDynAA();
  //errs() << "GetDynamicAAResults done\n";
  return false;
}

char GetDynamicAAResults::ID = 0;
//bool GetDynamicAAResults::DynAAStatus = true;
//SLVAPass GetDynamicAAResults::SLVA;
DenseMap<const Function *, unsigned> GetDynamicAAResults::FunctionIDMapping;
//DenseMap<unsigned, DenseSet<const Value *>> GetDynamicAAResults::AllPointers;
//DenseMap<unsigned, vector<const Value *>> GetDynamicAAResults::AllPointersVector;
//DenseMap<unsigned, DenseMap<ValuePair, LongIntPair>> GetDynamicAAResults::AliasValuePair;

//DenseMap<unsigned, DenseMap<const Value *, unsigned>> GetDynamicAAResults::AllValueIDMapping;
//DenseMap<unsigned, DenseMap<const Value *, unsigned>> GetDynamicAAResults::AllDeterministicValueIDMapping;
//DenseMap<unsigned, DenseMap<unsigned, Value *>> GetDynamicAAResults::AllDeterministicIDValueMapping;
//DenseSet<std::pair<const Value*, const Value*>> GetDynamicAAResults::MissCache;

static RegisterPass<GetDynamicAAResults> X("get-dynaa",
                                "Get the Combined results",
                                false, // Is CFG Only?
                                true); // Is Analysis?
