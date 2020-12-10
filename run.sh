#!/bin/bash

if [ -z "$1" ]
then
    echo "Needs .asm file"
else
    PC=0x0300
    BIN=$(basename $1 .asm).bin
    EMU=emu
    acme --cpu 6502 --setpc $PC -o $BIN $1
    g++ main.cpp -o $EMU
    ./$EMU $PC
    echo "-----------------"
    stat -c "SIZE: %5s BYTES" $BIN
    echo "-----------------"
    rm -f $EMU
    rm -f $BIN
fi
