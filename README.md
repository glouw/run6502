# RUN 6502

This is a fork of https://github.com/gianlucag/mos6502 that packages
assembling and running a single 6502 .asm file with PC wake up address
0x0300. The wake up address can be modified within run.sh.

## Usage

    ./run.sh code.asm

Note that the .asm file requires a JSR to main as its
first instruction, and that the main subroutine be terminated
with the RTS.

    JMP main ; MUST BE FIRST INSTRUCTION

    ; YOUR CODE GOES HERE

    main:
        ; YOUR CODE GOES HERE
        RTS ; MUST TERMINATE MAIN

Once the RTS of main is executed (read, the stack pointer is set to 0xFF),
the emulator will exit and the 6502 zero page will be printed.
