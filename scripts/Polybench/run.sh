#!/bin/bash
POLYBENCH=path_to_polybench_source
LLVM_ROOT_PATH=path_to_llvm_build_directory
CLANG=$LLVM_ROOT_PATH/bin/clang
OPT=$LLVM_ROOT_PATH/bin/opt
LINK=$LLVM_ROOT_PATH/bin/llvm-link
DYNAALIB=$LLVM_ROOT_PATH/lib/LLVMDynAA.so
ANALYSISLIB=$LLVM_ROOT_PATH/lib/libLLVMAnalysis.so
INSTRFUNC=$LLVM_ROOT_PATH/lib/Analysis/CMakeFiles/LLVMAnalysis.dir/InstrumentedFunctions.cpp.o
LOGS=$HOME/logs
NATIVE_FLAGS="-O3"
GET_FLAGS="-O3 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -c"
USE_FLAGS="-O3 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops -emit-llvm -c -g"
CLANG_FLAGS="-O3 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops"
GETPHASE_FLAGS="-O3 -mllvm -loop-vect-enabled"
FUNC_FILE=_fun_file
MERGED=_merged.bc

BENCHMARK="correlation covariance gemm gemver gesummv symm syr2k syrk trmm 2mm 3mm atax bicg doitgen mvt cholesky durbin gramschmidt lu ludcmp trisolv deriche floyd-warshall nussinov adi fdtd-2d heat-3d jacobi-1d jacobi-2d seidel-2d"

if [ $# -eq 0 ]
  then
    echo "Please specify an option (native/get-annotation/use-annotation)"
    exit 0
fi

for val in $BENCHMARK
do
	if [ $val == "correlation" ] || [ $val == "covariance" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I datamining/$val utilities/polybench.c datamining/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o datamining/$val/$val -lm
			echo "Executing " $val 
			cd datamining/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I datamining/$val datamining/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o datamining/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK datamining/$val/$val.bc utilities/polybench.bc -o datamining/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 datamining/$val/$val$MERGED -o datamining/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC datamining/$val/out.bc -o datamining/$val/$val -lm
			echo "Executing " $val 
			cd datamining/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	cd $POLYBENCH/
        	$CLANG $USE_FLAGS -I utilities -I datamining/$val linear-algebra/blas/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o datamining/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK datamining/$val/$val.bc utilities/polybench.bc -o datamining/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 datamining/$val/$val$MERGED -o datamining/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC datamining/$val/out.bc -o datamining/$val/$val -lm
			echo "Executing " $val 
			cd datamining/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    elif [ $val == "gemm" ] || [ $val == "gemver" ] || [ $val == "gesummv" ] || [ $val == "symm" ] || [ $val == "syr2k" ] || [ $val == "syrk" ] || [ $val == "trmm" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I linear-algebra/blas/$val utilities/polybench.c linear-algebra/blas/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/blas/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/blas/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I linear-algebra/blas/$val linear-algebra/blas/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/blas/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/blas/$val/$val.bc utilities/polybench.bc -o linear-algebra/blas/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 linear-algebra/blas/$val/$val$MERGED -o linear-algebra/blas/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC linear-algebra/blas/$val/out.bc -o linear-algebra/blas/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/blas/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	cd $POLYBENCH/
        	$CLANG $USE_FLAGS -I utilities -I linear-algebra/blas/$val linear-algebra/blas/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/blas/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/blas/$val/$val.bc utilities/polybench.bc -o linear-algebra/blas/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 linear-algebra/blas/$val/$val$MERGED -o linear-algebra/blas/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC linear-algebra/blas/$val/out.bc -o linear-algebra/blas/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/blas/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    elif [ $val == "2mm" ] || [ $val == "3mm" ] || [ $val == "atax" ] || [ $val == "bicg" ] || [ $val == "doitgen" ] || [ $val == "mvt" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I linear-algebra/kernels/$val utilities/polybench.c linear-algebra/kernels/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/kernels/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/kernels/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I linear-algebra/kernels/$val linear-algebra/kernels/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/kernels/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/kernels/$val/$val.bc utilities/polybench.bc -o linear-algebra/kernels/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 linear-algebra/kernels/$val/$val$MERGED -o linear-algebra/kernels/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC linear-algebra/kernels/$val/out.bc -o linear-algebra/kernels/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/kernels/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	cd $POLYBENCH/
        	$CLANG $USE_FLAGS -I utilities -I linear-algebra/kernels/$val linear-algebra/kernels/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/kernels/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/kernels/$val/$val.bc utilities/polybench.bc -o linear-algebra/kernels/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 linear-algebra/kernels/$val/$val$MERGED -o linear-algebra/kernels/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC linear-algebra/kernels/$val/out.bc -o linear-algebra/kernels/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/kernels/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    elif [ $val == "cholesky" ] || [ $val == "durbin" ] || [ $val == "gramschmidt" ] || [ $val == "lu" ] || [ $val == "ludcmp" ] || [ $val == "trisolv" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I linear-algebra/solvers/$val utilities/polybench.c linear-algebra/solvers/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/solvers/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/solvers/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I linear-algebra/solvers/$val linear-algebra/solvers/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/solvers/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/solvers/$val/$val.bc utilities/polybench.bc -o linear-algebra/solvers/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 linear-algebra/solvers/$val/$val$MERGED -o linear-algebra/solvers/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC linear-algebra/solvers/$val/out.bc -o linear-algebra/solvers/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/solvers/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	cd $POLYBENCH/
        	$CLANG $USE_FLAGS -I utilities -I linear-algebra/solvers/$val linear-algebra/solvers/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o linear-algebra/solvers/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK linear-algebra/solvers/$val/$val.bc utilities/polybench.bc -o linear-algebra/solvers/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 linear-algebra/solvers/$val/$val$MERGED -o linear-algebra/solvers/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC linear-algebra/solvers/$val/out.bc -o linear-algebra/solvers/$val/$val -lm
			echo "Executing " $val 
			cd linear-algebra/solvers/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    elif [ $val == "deriche" ] || [ $val == "floyd-warshall" ] || [ $val == "nussinov" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I medley/$val utilities/polybench.c medley/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o medley/$val/$val -lm
			echo "Executing " $val 
			cd medley/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I medley/$val medley/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o medley/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK medley/$val/$val.bc utilities/polybench.bc -o medley/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 medley/$val/$val$MERGED -o medley/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC medley/$val/out.bc -o medley/$val/$val -lm
			echo "Executing " $val 
			cd medley/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	$CLANG $USE_FLAGS -I utilities -I medley/$val medley/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o medley/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK medley/$val/$val.bc utilities/polybench.bc -o medley/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 medley/$val/$val$MERGED -o medley/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC medley/$val/out.bc -o medley/$val/$val -lm
			echo "Executing " $val 
			cd medley/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    elif [ $val == "adi" ] || [ $val == "fdtd-2d" ] || [ $val == "heat-3d" ] || [ $val == "jacobi-1d" ] || [ $val == "jacobi-2d" ] || [ $val == "seidel-2d" ];
    then
        if [ $1 == "native" ];
        then
        	cd $POLYBENCH/
            $CLANG $NATIVE_FLAGS -I utilities -I stencils/$val utilities/polybench.c stencils/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o stencils/$val/$val -lm
			echo "Executing " $val 
			cd stencils/$val/ && ./$val > $val.$1.txt 2>&1
			tail -1 $val.$1.txt
        elif [ $1 == "get-annotation" ];
        then
        	rm -r $LOGS/$val.txt
        	cd $POLYBENCH/
			$CLANG $GET_FLAGS -I utilities -I stencils/$val stencils/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o stencils/$val/$val.bc
			$CLANG $GET_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK stencils/$val/$val.bc utilities/polybench.bc -o stencils/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -prof-opt-selective -prof-func-file $val$FUNC_FILE -bench-name $val -dynaa -O0 stencils/$val/$val$MERGED -o stencils/$val/out.bc
			$CLANG $CLANG_FLAGS $INSTRFUNC stencils/$val/out.bc -o stencils/$val/$val -lm
			echo "Executing " $val 
			cd stencils/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        elif [ $1 == "use-annotation" ];
        then
        	cd $POLYBENCH/
        	$CLANG $USE_FLAGS -I utilities -I stencils/$val stencils/$val/$val.c -DPOLYBENCH_DUMP_ARRAYS -DEXTRALARGE_DATASET -DPOLYBENCH_TIME -o stencils/$val/$val.bc
			$CLANG $USE_FLAGS -I utilities utilities/polybench.c -DPOLYBENCH_TIME -o utilities/polybench.bc
			$LINK stencils/$val/$val.bc utilities/polybench.bc -o stencils/$val/$val$MERGED
			$OPT -load $DYNAALIB -load $ANALYSISLIB -get-opt-selective -get-func-file $val$FUNC_FILE -bench-name-read $val -get-dynaa -O0 stencils/$val/$val$MERGED -o stencils/$val/out.bc
			$CLANG $GETPHASE_FLAGS $INSTRFUNC stencils/$val/out.bc -o stencils/$val/$val -lm
			echo "Executing " $val 
			cd stencils/$val/ && ./$val > $val.$1.txt 2>&1
			diff <(head -n -1 $val.native.txt) <(head -n -1 $val.$1.txt)
			tail -1 $val.$1.txt
        fi
    else
            echo "Please enter valid benchmark name."
	fi
done