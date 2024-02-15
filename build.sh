clang -O0 -std=gnu2x src/main.c -o build/debug/lpp -Llib -lluajit -lm -Iinclude -Isrc -ggdb3 -Wl,--export-dynamic
