build_mode ?= debug
build_dir := build/${build_mode}

# the executables we will be building
lpp := ${build_dir}/lpp
commonlua := ${build_dir}/commonlua

# default rule (because it is the first one)
# which is just to build lpp
all: ${lpp}

# set src/ as a search directory for targets
VPATH = src

# collect c files for the common files, lpp, and commonlua
# and then generate their respective object and dependency
# file paths

common_c_files := $(wildcard src/*.cpp)
common_o_files := $(foreach file,$(common_c_files),${build_dir}/$(file:.cpp=.o))
common_d_files := $(common_o_files:.o=.d)

lpp_c_files := $(wildcard src/lpp/*.cpp)
lpp_o_files := $(foreach file,$(lpp_c_files),${build_dir}/$(file:.cpp=.o))
lpp_d_files := $(lpp_o_files:.o=.d)

commonlua_c_files := $(wildcard src/commonlua/*.cpp)
commonlua_o_files := $(foreach file,$(commonlua_c_files),${build_dir}/$(file:.cpp=.o))
commonlua_d_files := $(commonlua_o_files:.o=.d)

# clean up stuff we output
clean:
	-rm -r build/debug/*
	-rm -r src/generated/*

# set verbose to false unless it was already specified 
# on cmdline (eg. make verbose=true)
verbose ?= false

ifeq (${verbose},false)
	v := @
endif

# choose which stuff to use to build
# this should be adjustable later
compiler     := clang++
linker       := clang++
preprocessor := cpp

compiler_flags :=     \
	-std=c++20        \
	-Iinclude         \
	-Isrc             \
	-Wno-pointer-sign \
	-Wno-gnu-folding-constant \
	-Wno-\#warnings   \
	-Wno-switch

ifeq ($(build_mode),debug)
	compiler_flags += -O0 -ggdb3
else ifeq ($(build_mode),release)
	compiler_flags += -O2
endif

linker_flags := \
	-Llib       \
	-lluajit    \
	-lm         \
	-Wl,--export-dynamic

# print a success message
reset := \033[0m
green := \033[0;32m
blue  := \033[0;34m
define print
	@printf "$(green)$(1)$(reset) -> $(blue)$(2)$(reset)\n"
endef

# build lpp executable
${lpp}: ${lpp_o_files} ${common_o_files}
	$(v)${compiler} $^ ${linker_flags} -o $@
	@printf "$(blue)$@$(reset)\n"

# build commonlua executable
${commonlua}: ${commonlua_o_files} ${common_o_files}
	$(v)${compiler} $^ ${linker_flags} -o $@
	@printf "$(blue)$@$(reset)\n"

# generic rule for turning c files into object files
${build_dir}/%.o: %.cpp
	@mkdir -p $(@D) # ensure directories exist
	$(v)${compiler} ${compiler_flags} -c $<  -o $@
	$(call print,$<,$@)

# generic rule for turning c files into dependency files
${build_dir}/%.d: %.cpp
	@mkdir -p $(@D) # ensure directories exist
	$(v)${preprocessor} $< ${compiler_flags} -MM -MG -MT ${build_dir}/$*.o -o $@

# include the dependency files if they have 
# been generated. they are generated by the compiler when
# we compiler object files.
-include ${common_d_files} ${lpp_d_files} ${commonlua_d_files}

# generated file rules
# these use common lua to run scripts that have access to the same
# stuff lpp uses 
generated/metaenv.h \
src/generated/metaenv.h: src/lpp/metaenv.lua src/scripts/metaenv2c.lua ${commonlua}
	$(v)${commonlua} src/scripts/metaenv2c.lua
	$(call print,$<,$@)

# all of these are generated by a single invocation
# of the tokens script
src/generated/token.map.h        \
src/generated/token.enum.h       \
src/generated/token.strings.h    \
src/generated/tokens.stringmap.h \
&: src/scripts/tokens.lua ${commonlua}
	$(v)${commonlua} $<
	$(call print,$<,$@)

# disable make's dumb builtin rules for performance
MAKEFLAGS += --no-builtin-rules
