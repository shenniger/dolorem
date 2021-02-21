all: dolorem chdrconv

CC = gcc
CXX = g++

MODE ?= -g -O0
CFLAGS ?=
CXXFLAGS ?= 
LINKFLAGS ?=

LLVMPREFIX ?= ~
LIBCLANGPREFIX ?= /usr/lib/llvm-3.9

%.o: %.c *.h
	$(CC) -c -Wall -Wextra -fpic -pedantic -std=c11 -isystem $(LLVMPREFIX)/include  $< -o $@ $(MODE) $(CFLAGS)

llvmext.o: llvmext.cpp llvmext.h
	$(CXX) -c -Wall -Wextra -fpic -pedantic -std=c++17 -isystem $(LLVMPREFIX)/include  $< -o $@ $(MODE) $(CXXFLAGS) -fno-exceptions -fno-rtti

libdolorem.so: list.o jit.o hashmap.o type.o fun.o basictypes.o eval.o global.o var.o quote.o include.o llvmext.o structs.o
	$(CXX) $^ -o $@ -shared $(LINKFLAGS)

dolorem: main.o libdolorem.so
	$(CXX) -L. $^ -o $@ -fno-rtti -fno-exceptions -L $(LLVMPREFIX)/lib -lLLVM -ldl $(MODE) $(LINKFLAGS)

chdrconv.o: chdrconv.c
	$(CC) -c -Wall -Wextra -pedantic -std=c99 $(CFLAGS) $< -o $@ $(MODE) -isystem $(LIBCLANGPREFIX)/include

chdrconv: chdrconv.o
	$(CXX) $^ -o $@ $(MODE) $(LINKFLAGS) -L $(LIBCLANGPREFIX)/lib -lclang -fno-rtti -fno-exceptions

.PHONY: report clean
report:
	@echo Using C compiler: $(CC)
	@echo Using C++/LLVM compiler: $(CXX)
	@echo Using LLVM version: $(shell $(LLVMPREFIX)/bin/llvm-config --version)
	@echo Debug/Release flags: $(MODE)
	@echo Other flags \(CFLAGS\): $(CFLAGS)
	@echo Other flags \(CXXFLAGS\): $(CXXFLAGS)
	@echo Other flags \(LINKFLAGS\): $(LINKFLAGS)

clean:
	rm *.o || true
	rm test || true
	rm libtest.so || true
