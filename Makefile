all: dolorem

CC = gcc-new
CXX = g++-new

MODE ?= -g -O0
CFLAGS ?=
CXXFLAGS ?= 
LINKFLAGS ?=

LLVMPREFIX ?= ~/local

%.o: %.c *.h
	$(CC) -c -Wall -Wextra -fpic -pedantic -std=c11 -isystem $(LLVMPREFIX)/include  $< -o $@ $(MODE) $(CFLAGS)

llvmext.o: llvmext.cpp llvmext.h
	$(CXX) -c -Wall -Wextra -fpic -pedantic -std=c++17 -isystem $(LLVMPREFIX)/include  $< -o $@ $(MODE) $(CXXFLAGS) -fno-exceptions -fno-rtti

libdolorem.so: list.o jit.o hashmap.o type.o fun.o basictypes.o eval.o global.o var.o quote.o include.o llvmext.o structs.o
	$(CXX) $^ -o $@ -shared

dolorem: main.o libdolorem.so
	$(CXX) -L. $^ -o $@ -fno-rtti -fno-exceptions \
		-L $(LLVMPREFIX)/lib -lLLVM -ldl $(MODE) $(LINKFLAGS)

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
