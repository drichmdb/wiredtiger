#!/bin/bash

# Single use script to add an include for the new _private.h files in any place where they *might* be needed. 
# TODO - Some of these includes will be redundant. Adding it everywhere for the PoC

for comp in */*_private.h; do
    folder=$(dirname "$comp")
    cd "$folder" || exit 1
        for F in *.c; do sed -i "/#include \"wt_internal.h\"/a\#include \"${folder}_private.h\"" "$F"; done
    cd "../"
done

