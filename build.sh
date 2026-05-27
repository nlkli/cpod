#!/bin/bash

set -e

mkdir -p ./bin

clang -Wall \
    -Wextra \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib \
    -lavformat \
    -lavcodec \
    -lswresample \
    -lavutil \
    -framework AudioToolbox \
    -framework CoreAudio \
    ./src/main.c -o ./bin/cpod
