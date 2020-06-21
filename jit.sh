#!/bin/bash

make clean
make clean_mfs
make
./server 10000 my_fsi
