The following should be installed before building Horizon:

cmake: sudo apt-get install cmake

ninja: sudo apt install ninja-build

libffi: sudo apt-get install -y libffi-dev

Building Horizon

1) Download LLVM's source folder (llvm) included in this repository.

2) Clone clang's source folder to the tools directory present in the llvm folder using the following command:

   git clone https://github.com/llvm-mirror/clang
   
3) Clone source folder of compiler-rt to the projects directory present in the llvm folder using the following command:

   git clone https://github.com/llvm-mirror/compiler-rt

4) Create a new folder to build llvm (e.g. build).

5) Place the build.sh file present in the scripts folder to llvm’s build folder.

6) Specify the path to llvm’s source folder (downloaded in step 1) in the build.sh file.

7) Go to the build folder and execute the following commands to build llvm:

   ./build.sh

   ninja


Function files

1) Create a folder named logs in the $HOME directory.

2) Place all the files present in the folder FunctionFiles to $HOME/logs.


Polybench benchmarks

1) Download the source code for Polybench benchmarks from the following link:

https://sourceforge.net/projects/polybench/

2) Place the run.sh file present in scripts/Polybench folder to the source folder of Polybench (downloaded in the previous step).

3) Set the value of the variable POLYBENCH in the run.sh file to the path of Polybench’s source.

4) Set the value of the variable LLVM_ROOT_PATH in the run.sh file to the path of llvm’s build folder.

5) Set the variable BENCHMARK to the benchmark name in the run.sh file to execute the specific benchmarks. For example, BENCHMARK="jacobi-1d". The default value will execute all the benchmarks of polybench benchmark suite.

6) To execute the native run,

./run.sh native

7) To execute the annotation inference phase,

./run.sh get-annotation

8) To execute the annotation incorporation phase,

./run.sh use-annotation


SPLASH-2 benchmarks

1) Download SPLASH-2 benchmarks by following the link:

https://docs.google.com/document/d/1B7nZSqMLwkwoVNEj_58tMPTk4bKWvoEMbokOAjqeC-k/preview

Follow the "Get SPLASH-2" and "Patch SPLASH-2" sections from the document. 

2) Replace Makefile.config with the one present in the scripts/SPLASH-2 folder. 

3) Replace Makefile (apps/radiosity) with the one present in the scripts/SPLASH-2 folder. 

4) Set the value of the variable LLVM_ROOT_PATH in the Makefile.config file to the path of llvm’s build folder.

5) Set the value of the variable BASEDIR in the Makefile.config file to the path of SPLASH-2’s source directory.

6) Place the splashrun.sh file present in scripts/SPLASH-2 folder into the source folder of SPLASH-2 (where Makefile.config is present).

7) Set the variable BENCHMARK to the benchmark name in the splashrun.sh file to execute the specific benchmarks.

8) To execute the native run,

./splashrun.sh native

9) To execute the annotation inference phase,

./splashrun.sh get-annotation

10) To execute the annotation incorporation phase,

./splashrun.sh use-annotation

11) The specific parameters (-p) of the benchmarks can be modified to execute the benchmarks with different numbers of threads.


CPU SPEC 2017 benchmarks

1) Place all the config (.cfg extension) files present in scripts/SPEC into the config folder of SPEC.

2) Replace Makefile.defaults present in benchspec with the Makefile.defaults present in scripts/SPEC.

3) Place the specrun.sh file present in scripts/SPEC into the base folder of SPEC.

4) Set the variable BENCHMARK to execute the specific benchmarks.

5) To execute the native run,

./specrun.sh native

6) To execute the annotation inference phase,

./specrun.sh get-annotation

7) To execute the annotation incorporation phase,

./specrun.sh use-annotation

Note: While executing a script, if you get the "Permission denied" error, execute the following command before and then execute the script:

chmod +x script-name
