#!/bin/bash

rm -f test test.o
clang++ -c -o test.o test.cpp -std=c++11
clang++ -o test test.o
./test
