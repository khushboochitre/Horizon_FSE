add_llvm_library(LLVMDynAA MODULE
  DynAA.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Analysis
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Analysis/DynAA
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
	opt
  )
