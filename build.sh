clang -O0 -std=gnu2x src/main.c -o build/debug/main.o -Llib -lluajit -lm -Iinclude -Isrc -ggdb3 -Wl,--export-dynamic &
clang -O0 -std=gnu2x src/lpp.c -o build/debug/lpp.o -Llib -lluajit -lm -Iinclude -Isrc -ggdb3 -Wl,--export-dynamic &
clang -O0 -std=gnu2x src/lex.c -o build/debug/lex.o -Llib -lluajit -lm -Iinclude -Isrc -ggdb3 -Wl,--export-dynamic &
clang -O0 -std=gnu2x src/common.c -o build/debug/lex.o -Llib -lluajit -lm -Iinclude -Isrc -ggdb3 -Wl,--export-dynamic &
wait
clang main.o lex.o lpp.o
