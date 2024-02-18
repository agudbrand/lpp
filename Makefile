CFLAGS = -c -O0 -g  -Iinclude -Isrc -ggdb3 
LFLAGS = -Llib -lluajit -lm -Wl,--export-dynamic

# CFILES := $(wildcard src/*.c)
# OBJS := $(CFILES:src/%.c=build/debug/%.o)

%.o: %.c
	echo $(input) $(output)

