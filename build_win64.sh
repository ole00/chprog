#!/bin/bash

CC=x86_64-w64-mingw32-gcc
FLAGS="-s -Iports/include -Lports/lib/win64 -Wno-deprecated-declarations"

$CC  $FLAGS main.c prot_v2.c prot_v1.c -lusb-1.0 -o chprog.exe
