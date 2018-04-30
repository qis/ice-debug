MAKEFLAGS += --no-print-directory

CC	!= which clang-devel || which clang
CXX	!= which clang++-devel || which clang++
TIDY	!= which clang-tidy-devel || which clang-tidy
FORMAT	!= which clang-format-devel || which clang-format
SYSDBG	!= which lldb || which gdb
CMAKE	!= which cmake

CONFIG	:= $(CMAKE) -GNinja -DCMAKE_TOOLCHAIN_FILE:PATH=${VCPKG}/scripts/buildsystems/vcpkg.cmake
CONFIG	+= -DVCPKG_TARGET_TRIPLET=${VCPKG_DEFAULT_TRIPLET}
CONFIG	+= -DEXTRA_WARNINGS:BOOL=ON
CONFIG	+= -DBUILD_EXAMPLES:BOOL=ON
CONFIG	+= -DBUILD_SSH:BOOL=ON

PROJECT	!= grep "^project" CMakeLists.txt | cut -c9- | cut -d " " -f1 | tr "[:upper:]" "[:lower:]"
SOURCES	!= find src -type f -name '*.h' -or -name '*.cpp'

all: debug

run: build/llvm/debug/CMakeCache.txt
	@@cd build/llvm/debug && $(CMAKE) --build . --target main && ./main

dbg: build/llvm/debug/CMakeCache.txt
	@@cd build/llvm/debug && $(CMAKE) --build . --target main && $(SYSDBG) ./main

test: build/llvm/debug/CMakeCache.txt
	@cd build/llvm/debug && $(CMAKE) --build . --target tests && ctest

tidy:
	@clang-tidy -p build/llvm/debug $(SOURCES) -header-filter=src

format:
	@$(FORMAT) -i $(SOURCES)

debug: build/llvm/debug/CMakeCache.txt $(SOURCES)
	@$(CMAKE) --build build/llvm/debug

release: build/llvm/release/CMakeCache.txt $(SOURCES)
	@$(CMAKE) --build build/llvm/release

build/llvm/debug/CMakeCache.txt: CMakeLists.txt build/llvm/debug
	@cd build/llvm/debug && CC=$(CC) CXX=$(CXX) $(CMAKE) -DCMAKE_BUILD_TYPE=Debug $(CONFIG) $(PWD)

build/llvm/release/CMakeCache.txt: CMakeLists.txt build/llvm/release
	@cd build/llvm/release && CC=$(CC) CXX=$(CXX) $(CMAKE) -DCMAKE_BUILD_TYPE=Release $(CONFIG) $(PWD)

build/llvm/debug:
	@mkdir -p build/llvm/debug

build/llvm/release:
	@mkdir -p build/llvm/release

clean:
	@rm -rf build/llvm bin lib

.PHONY: all run dbg test tidy format debug release clean
