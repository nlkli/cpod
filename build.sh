#!/bin/bash

set -e

clang -Wall \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib \
    -lavformat \
    -lavcodec \
    -lavutil \
    -lswresample \
    -framework AudioToolbox \
    -framework CoreAudio \
    main.c -o cpod
