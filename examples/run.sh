#!/bin/bash

KLEE=~/tau/slicing/klee_build/bin/klee
LIBS_PATH=~/tau/slicing/SVF/build/lib:~/tau/slicing/SVF/build/lib/CUDD:~/tau/slicing/dg/build/src

function run_klee {
    file=$1
    slice=$2
    search=$3
    LD_LIBRARY_PATH=${LIBS_PATH} ${KLEE} -libc=uclibc --posix-runtime -search=${search} -slice=${slice} ${file} 1>/dev/null 2>/dev/null
    if [ $? != 0 ]; then
        echo "failed: ${file}"
        exit 1
    fi
}

function run_file {
    file=$1
    slice=$2
    run_klee ${file} ${slice} dfs
    run_klee ${file} ${slice} bfs
    echo "${file}: OK"
}

run_file e0/final.bc f
run_file e1/final.bc target
run_file e2/final.bc target
run_file e3/final.bc check_token
run_file e4/final.bc parser_parse_name
run_file e5/final.bc parser_parse_tokens
