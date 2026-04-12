# Jacob Chisholm

.PHONY: all build clean format

BUILD_DIR := build
BUILD_TYPE ?= Debug

PROJ_NAME = $(shell grep "CMAKE_PROJECT_NAME:STATIC=" ${BUILD_DIR}/CMakeCache.txt 2>/dev/null | cut -d'=' -f2)

all: build

gen_cmake_host:
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-linux-gnu-toolchain.cmake \
		-B${BUILD_DIR} \
		-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-G "Unix Makefiles" 

gen_cmake_bf:
	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu-toolchain.cmake \
		-B${BUILD_DIR}_BF \
		-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-G "Unix Makefiles" 

build: gen_cmake_host
	$(MAKE) -C ${BUILD_DIR} --no-print-directory -j$(nproc)

build_bf: gen_cmake_bf
	$(MAKE) -C ${BUILD_DIR}_BF --no-print-directory -j$(nproc)

SRCS := $(shell find . -name '*.[ch]' -or -name '*.[ch]pp')
%.format: %
	clang-format -i $<
format: $(addsuffix .format, ${SRCS})

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BUILD_DIR)_BF
