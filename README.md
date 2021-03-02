Building Horizon

1) Clone the llvm git repository.

https://github.com/llvm-mirror/llvm

2) Clone clang to the tools directory present in the llvm folder.

https://github.com/llvm-mirror/clang

3) Copy the contents (files and folders) present in src/lib to the cloned llvm folder (lib/Analysis). Replace the CMakeLists.txt present in the cloned llvm folder (lib/Analysis).

4) Copy the DynAA folder present in src/include to the cloned llvm folder (include/llvm/Analysis).

5) Create a new folder to build llvm (e.g. build).

6) Place the build.sh file present in the scripts folder to llvm’s build folder.

7) Specify the path to llvm’s source code in the build.sh file.

8) Go to the build folder and execute the following commands to build llvm:

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

5) Set the variable BENCHMARK to execute the specific benchmarks.

6) To execute the native run,

./run.sh native

7) To execute the annotation inference phase,

./run.sh get-annotation

8) To execute the annotation incorporation phase,

./run.sh use-annotation


SPLASH-2 benchmarks

1) Download the source code for the SPLASH-2 benchmarks using the following link:

http://web.archive.org/web/20070704172333/http://www-flash.stanford.edu/apps/SPLASH/splash2.tar.gz

2) Replace Makefile.config with the one present in the scripts/SPLASH-2 folder. 

3) Replace Makefile (apps/radiosity) with the one present in the scripts/SPLASH-2 folder. 

4) Set the value of the variable LLVM_ROOT_PATH in the Makefile.config file to the path of llvm’s build folder.

5) Set the value of the variable BASEDIR in the run.sh file to the path of SPLASH-2’s source directory.

6) Place the splashrun.sh file present in scripts/SPLASH-2 folder into the source folder of SPLASH-2.

7) Set the variable BENCHMARK to execute the specific benchmarks.

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

