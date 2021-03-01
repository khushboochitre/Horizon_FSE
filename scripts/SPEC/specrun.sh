#!/bin/bash
source shrc
SIZE="ref"
BENCHMARK="519.lbm_r 538.imagick_r 544.nab_r 505.mcf_r 557.xz_r 525.x264_r 500.perlbench_r 502.gcc_r"

for arg
do
        if [ $arg == "native" ];
        then
                echo "Native"
                runcpu --config=native --noreportable --tune=base --copies=1 --iteration=1 --action=clobber --size=$SIZE $BENCHMARK
                runcpu --config=native --noreportable --tune=base --copies=1 --iteration=1 --size=$SIZE $BENCHMARK
        elif [ $arg == "get-annotation" ];
        then
                echo "Annotation Inference"
                runcpu --config=get-annotation --noreportable --tune=base --copies=1 --iteration=1 --action=clobber --size=$SIZE $BENCHMARK
                runcpu --config=get-annotation --noreportable --tune=base --copies=1 --iteration=1 --size=$SIZE $BENCHMARK
        elif [ $arg == "use-annotation" ];
        then
                echo "Annotation Incorporation"
                runcpu --config=use-annotation --noreportable --tune=base --copies=1 --iteration=1 --action=clobber --size=$SIZE $BENCHMARK
                runcpu --config=use-annotation --noreportable --tune=base --copies=1 --iteration=1 --size=$SIZE $BENCHMARK
        else
                echo "Please specify atleast one argument. (native/get-annotation/use-annotation)";
        fi
done
