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

for f in $(ls dynalloc*.bc); do
    run_file $f foo
done
for f in $(ls funcarray*.bc); do
    run_file $f call
done
for f in $(ls global*.bc); do
    run_file $f foo
done
for f in $(ls interprocedural*.bc); do
    run_file $f foo
done
for f in $(ls list*.bc); do
    run_file $f insert_list
done
