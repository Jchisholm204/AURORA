#!/usr/bin/env zsh

local BUILD_ARCH_DEFAULT="x86_64"
# Leave this OFF, some tests only build on x86_64
local BUILD_TESTS_DEFAULT="OFF"

function build_gen_cmake() {
    local BUILD_ARCH=$1
    local BUILD_DIR=$2
    local BUILD_TESTS=$3
    if [[ ! $BUILD_ARCH ]]; then
        printf "Build Architecture Not Specified.\n" >& 2
        return 1
    fi
    if [[ ! $BUILD_DIR ]]; then
        printf "Build Directory Not Specified.\n" >& 2
        return 1
    fi
    if [[ $BUILD_TESTS != "ON" ]]; then
        local BUILD_TESTS=$BUILD_TESTS_DEFAULT
    fi
    printf "Build Tests %s.\n" $BUILD_TESTS

    local PDIR=$(pwd)
    cd "${AURORA_TOP_DIR}"

	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/${BUILD_ARCH}-linux-gnu-toolchain.cmake \
		-B${BUILD_DIR} \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_TESTING=${BUILD_TESTS} \
		-G "Unix Makefiles" 
    cd "${PDIR}"
}

function build_make() {
    local BUILD_DIR=$1
    local PROCS=$2
    if [[ ! $BUILD_DIR ]]; then
        printf "Build Dir Not Specified.\n" >& 2
        return 1
    fi
    if [[ ! $PROCS ]]; then 
        local PROCS=$(( $(nproc) / 4 ))
        printf "N Processes not specified. Using %d\n" $PROCS
    fi
    make -C ${BUILD_DIR} --no-print-directory -j${PROCS}
}

function build() {
    local BUILD_ARCH=$1
    local BUILD_TESTS=$2
    local BUILD_DIR=$3
    if [[ ! $BUILD_ARCH ]]; then
        printf "Build Architecture Not Specified.\n" >& 2
        printf "Defaulting to: %s\n" $BUILD_ARCH_DEFAULT >& 2
        local BUILD_ARCH=$BUILD_ARCH_DEFAULT
    fi
    if [[ ! $BUILD_DIR ]]; then 
        local BUILD_DIR="build_${BUILD_ARCH}"
    fi
    if [[ $BUILD_TESTS != "ON" ]]; then
        local BUILD_TESTS= $BUILD_TESTS_DEFAULT
    fi
    printf "Building AURORA: %s\n" $BUILD_ARCH

    # Source Cluster ENV
    source $AURORA_CLUSTER_DIR/env.sh $BUILD_ARCH

    build_gen_cmake $BUILD_ARCH $BUILD_DIR $BUILD_TESTS
    build_make $BUILD_DIR
}

# Example builder
function _test_build_all() {
    build "x86_64" "ON"
    build "aarch64" "OFF"
}

# -- BUILD --

local BUILD_ARCH=$1
local BUILD_TESTS=$2
local BUILD_DIR=$3

if [[ ! $BUILD_ARCH ]]; then
    local BUILD_ARCH=$BUILD_ARCH_DEFAULT
fi

if [[ $BUILD_TESTS != "ON" ]]; then
    local BUILD_TESTS=$BUILD_TESTS_DEFAULT
fi

build $BUILD_ARCH $BUILD_TESTS $BUILD_DIR

# -- BUILD --
