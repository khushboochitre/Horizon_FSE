#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/DynAA/DynAA.h"

using namespace std;
using namespace llvm;

typedef pair<unsigned long long, unsigned long long> LongIntPair;
typedef pair<unsigned, unsigned> IntPair;
typedef pair<const Value *, const Value *> ValuePair;

namespace daa {

	struct FuncSignature {
		string Name;
		unsigned NumArgs;
		unsigned NumPointers;
		unsigned MaxID;
	};

	struct GetDynamicAAResults : public ModulePass {
		static char ID;
		IntegerType *CharType, *IntType;
    	Type *VoidType, *Int64Ty;
   		PointerType *CharStarType, *Int64PtrTy;
		Function *CheckMustPairFunc, *DebugInfo, *CheckNoPairFunc;
		Function *NoAliasIntrinsic;
		GlobalVariable* GV;
		GlobalVariable* TGV;

		typedef DenseMap<const Value*, unsigned> ValueIDMapTy;
		GetDynamicAAResults();
		virtual bool runOnModule(Module& M);
  		virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		private: 
			void setupScalarTypes(Module &M);
			void addFuncDef(Module &M);
			void loadDynamicAAResultsToMap(
				DenseMap<unsigned, 
				DenseMap<IntPair, LongIntPair>> &Aliases);
			void verifyArgs(Function *F, 
				DenseMap<unsigned, DenseMap<IntPair, LongIntPair>> &AllAliases, 
				unsigned FID, ValueIDMapTy ValueIDMapping, 
				DenseMap<Value*, bool> &Args, DenseMap<Instruction *, bool> &DefInst);
			void createNewDebugIR(Function *F, DenseMap<int, int> MustAliasMap, 
  				DenseMap<std::pair<int, int>, unsigned long long> NoAliasMap,
  				ValueIDMapTy ValueIDMapping, DenseMap<Value *, uint64_t> &PtrSizeMap);
			bool verifyFuncSignature(unsigned FID, Function *F, 
				unsigned MaxID, int NumDefs, int NumArgs);

		    DenseMap<unsigned, Value *> IDValueMapping;
		    DenseMap<unsigned, FuncSignature> FunctionSignaturesMap;

		    static DenseMap<const Function *, unsigned> FunctionIDMapping;
	};
}
