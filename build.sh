#!/bin/bash

set -e

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
    main.c -o cpod
