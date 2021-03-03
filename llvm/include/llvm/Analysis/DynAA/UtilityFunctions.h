#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <time.h>

#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/PHITransAddr.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/IR/PredIteratorCache.h"

#define DEBUG_TYPE "dynaa"

using namespace llvm;
using namespace std;

#define NO_ALIAS_TYPE  0
#define MAY_ALIAS_TYPE 1

static const unsigned InvalidID = -1;
static clock_t PrevTime = 0;
bool dynAADominates(Value *V1, Value *V2, DominatorTree *DT);

namespace daa {

	using LiveVar = DenseSet<Value *>;
	class LiveAnalysis {
    	private:
			DenseMap< Instruction *, LiveVar> In, Out;

       	public:			
       		bool run(Function &F); 
			DenseMap< Instruction *, LiveVar> returnInSet() { return In; }
			DenseMap< Instruction *, LiveVar> returnOutSet() { return Out; }

			bool computeInSet(DenseSet<Value*> &Res, BasicBlock *BB, DenseMap<BasicBlock*, int> Visited, bool &Changed);
			bool isLiveBefore( Instruction *I, Value *V);
			bool isLiveAfter( Instruction *I, Value *V);
			void pOut( Instruction *I);
			void pIn( Instruction *I);
			void print(LiveVar jset);
			void printBBInOut(Function &F);
	};

	//class SLVAPass : public FunctionPass {
	class SLVAPass{
		private:
			DenseMap< Instruction *, LiveVar> InSet;
			DenseMap< Instruction *, LiveVar> OutSet;

	    public:
			static const unsigned InvalidID = -1;

			SLVAPass();
			virtual ~SLVAPass();
			void run(Function &F);
			
			void printBBInOut(Function &F);
			void printValueIDMap(unsigned FID, Function *F, DenseMap<const Value *, unsigned> ValueIDMapping);
			void printIDValueMap(unsigned FID, Function *F, DenseMap<unsigned, Value *> IDValueMapping);
			void printFunctionIDMap(DenseMap<const Function *, unsigned> FunctionIDMapping);

			LiveVar returnInSet( Instruction *I) { return InSet[I]; }
			LiveVar returnOutSet( Instruction *I) { return OutSet[I]; }

			unsigned assignUniqueID(Function *F, DenseMap<const Value *, unsigned> 
				&ValueIDMapping, DenseMap<unsigned, Value *> &IDValueMapping);

			bool addToValueMap(unsigned VID, Value *V, DenseMap<const Value *, unsigned> 
				&ValueIDMapping, DenseMap<unsigned, Value *> &IDValueMapping);
			bool addToFunctionMap(unsigned ID, const Function *F, DenseMap<const Function *, 
				unsigned> &FunctionIDMapping);
			unsigned getValueID(const Value *V, DenseMap<const Value *, unsigned> ValueIDMapping);
			Value *getIDValue(unsigned VID, DenseMap<unsigned, Value *> IDValueMapping);
			unsigned getFunctionID(const Function *F, DenseMap<const Function *, unsigned> FunctionIDMapping);
			bool canTrackAliasInFunc(const Function *F);
			bool canTrackAliasInArg(const Value *V);
			bool canTrackNoAliasInInst(const Instruction *I);
			bool canTrackAliasInInst(const Instruction *I, bool Selective);
			void getDebugInfo1(Function *DebugInfo, GlobalVariable *TGV, DenseMap<const Value*, unsigned> ValueIDMapping, 
				unsigned FID, Function *F, DenseSet<Value*> PtrOperands, DenseSet<Value*> Bases);
			void replaceUndefCall(Function *F);
			void readFunctionsFromFile(string Filename, std::map<string, unsigned> &NameToIDMap);
			void mapFunctionsToID(Module &M, string Filename, DenseMap<const Function *, unsigned> &FunctionIDMapping);
			AliasResult checkAliasMetadata(Function *F, const MemoryLocation &LocA, const MemoryLocation &LocB, AAResults *AA, unsigned *MissedQueries);

			void getPointersInDefInst(Instruction *I, bool SelectiveInst, DenseMap<Instruction *, bool> &DefInst, DenseMap<Value *, bool> &Args);

			Value* fetchPointerOperand(Instruction *I);
			Value* fetchSourcePointerOperand(Instruction *I);
			void isArgorInst(Value *V, DenseMap<Instruction *, bool> &DefInst, DenseMap<Value *, bool> &Args, bool isOperand);

			void getSize(const DataLayout* DL, Value *V, uint64_t *TP);
			AliasResult getStaticAAResult(DataLayout* DL, Value *V1, Value *V2, DenseMap<Instruction *, bool> DefInst, DenseMap<Value *, bool> Args, AAResults &AA);
			void getExtraOperands(Instruction *I, BasicBlock *StartBB, const PHITransAddr &Pointer, 
		  		DenseSet<Value *> &TmpPtrOperands, DenseMap<BasicBlock *, Value *> &Visited, 
		  		DominatorTree *DT, AssumptionCache &AC, DenseMap<Value *, uint64_t> &PtrSizeMap, uint64_t size);
			void removeInvalidMetadata(Function *F);
	};
}
