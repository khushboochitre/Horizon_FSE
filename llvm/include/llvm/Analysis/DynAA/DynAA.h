#include "llvm/Pass.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/OrderedInstructions.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Analysis/DynAA/UtilityFunctions.h"
#include "llvm/IR/LLVMContext.h"

using namespace llvm;
using namespace std;

namespace daa {
	struct DynAA : public ModulePass {
		static char ID;

		DynAA();
		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	  	virtual bool runOnModule(Module& M);

	private:
		IntegerType *CharType, *IntType;
    	Type *VoidType, *Int64Ty, *Int1Ty;
   		PointerType *CharStarType, *Int64PtrTy, *Int1PtrTy;
		Function *FileWrite, *BasePtrHook, *DebugInfo, *PrintValAddress, *GetAliasInfo;

		GlobalVariable* GV;
		GlobalVariable* TGV;
		typedef DenseMap<const Value*, unsigned> ValueIDMapTy;
		DenseMap<unsigned, GlobalVariable *> FuncGlobalVarMap;

		void setUpHooks(Module &M);
		void setupScalarTypes(Module &M);
		Value* instrumentFuncEntry(Function *F, int FID, 
			int MaxID, int ArgSize, int PointerSize);
		void insertChecksLoc1(Function *F, unsigned FID, 
			unsigned MaxID, Value *Ptr, Value *MVar, Value* Base, 
  			SLVAPass &SLVA, Instruction *Loc, 
			ValueIDMapTy ValueIDMapping, bool CheckPartial);
		void insertChecksLoc2(Function *F, unsigned FID, unsigned MaxID, 
			Value *Ptr, Value *MVar, Value* Base, 
  			SLVAPass &SLVA, Instruction *Loc, 
			ValueIDMapTy ValueIDMapping, bool CheckPartial);
		void insertChecks1(Function *F, unsigned FID, unsigned MaxID,
			Value *Ptr, Value *MVar, Value* &BasePtr, 
  			SLVAPass &SLVA, int NumArgs, int NumPointers, 
			ValueIDMapTy ValueIDMapping, bool CheckPartial);
		void printValueAddress(unsigned FID, unsigned VID1, unsigned VID2, 
			Value *Def, Value *MVar, Instruction *Loc, unsigned S1, unsigned S2);
		void getMayPairs(Function *F, SLVAPass &SLVA, unsigned FID, DenseSet<Value *> Ptrs, 
			AAResults &AA, bool CheckPartial, DenseSet<std::pair<Value*, Value*>> &MayPairs,
			ValueIDMapTy ValueIDMapping, DenseSet<std::pair<unsigned, unsigned>> &LivePointerPairs, 
			DenseMap<Value *, uint64_t> PtrSizeMap);
		void instrumentReturn(Function *F, DenseSet<Instruction *> RetInst);
	};
}
