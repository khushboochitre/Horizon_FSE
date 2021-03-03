#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CallSite.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;
using namespace std;

namespace daa {
	struct UniqueIDAssigner: public ModulePass {
		static char ID;
	    static const unsigned InvalidID = -1;

	    UniqueIDAssigner();
	    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	    virtual bool runOnModule(Module& M);
	    unsigned getFunctionID(const Function *F);
	    unsigned getValueID(unsigned FID, Value *V);
	    Value *getIDValue(unsigned FID, unsigned ID);

	private:
		bool addToFunctionMap(unsigned ID, Function *F);
		bool addToValueMap(unsigned FID, unsigned VID, Value *V);

		DenseMap<Function *, unsigned> FunctionIDMapping;
		DenseMap<unsigned, DenseMap<Value *, unsigned>> ValueIDMapping;
	    DenseMap<unsigned, DenseMap<unsigned, Value *>> IDValueMapping;
	};
}