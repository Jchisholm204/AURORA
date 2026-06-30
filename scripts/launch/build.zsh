#!/usr/bin/env zsh

# launch/build.zsh
# Automatically builds the required executables to run tests

local BUILD_ARCH_DEFAULT="x86_64"
# Leave this OFF, some tests only build on x86_64
local BUILD_TESTS_DEFAULT="OFF"

function build_gen_cmake() {
    local BUILD_ARCH=$1
    local BUILD_DIR=$2
    local BUILD_TESTS=$3
    # Optional Parameters
    local ACR_MAX_WORKERS=$4
    local AIM_MAX_WORKERS=$5

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

    if [[ $ACR_MAX_WORKERS == <-> ]]; then
        ACR_MAX_WORKERS="-DACR_MAX_WORKERS=${ACR_MAX_WORKERS}"
    fi
    if [[ $AIM_MAX_WORKERS == <-> ]]; then
        AIM_MAX_WORKERS="-DAIM_MAX_WORKERS=${AIM_MAX_WORKERS}"
    fi

    local PDIR=$(pwd)
    cd "${AURORA_TOP_DIR}"

	cmake \
		-DCMAKE_TOOLCHAIN_FILE=cmake/${BUILD_ARCH}-linux-gnu-toolchain.cmake \
		-B${BUILD_DIR} \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DBUILD_TESTING=${BUILD_TESTS} \
        ${ACR_MAX_WORKERS} \
        ${AIM_MAX_WORKERS} \
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
    # Optional Parameters
    local ACR_MAX_WORKERS=$4
    local AIM_MAX_WORKERS=$5
    
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

    build_gen_cmake \
        $BUILD_ARCH \
        $BUILD_DIR \
        $BUILD_TESTS \
        $ACR_MAX_WORKERS \
        $AIM_MAX_WORKERS 

    build_make \
        $BUILD_DIR
}

# Example builder
function _test_build_all() {
    build "x86_64" "ON"
    build "aarch64" "OFF"
}

# -- BUILD --

local TEST_BUILD_DIR=$1
local BACKEND_BUILD_DIR=$2
# Optional Parameters
local ACR_MAX_WORKERS=$3
local AIM_MAX_WORKERS=$4

if [[ "${AURORA_BACKEND_PLATFORM}" == 'bf' ]]; then
    rm -rf "$TEST_BUILD_DIR"
    rm -rf "$BACKEND_BUILD_DIR"
    build \
        'x86_64' \
        'ON' \
        $TEST_BUILD_DIR \
        $ACR_MAX_WORKERS \
        $AIM_MAX_WORKERS
    build \
        'AArch64' \
        'OFF' \
        $BACKEND_BUILD_DIR \
        $ACR_MAX_WORKERS \
        $AIM_MAX_WORKERS
# elif [[ "${aurora_backend_platform}" == 'host' ]]; then
else
    rm -rf "$TEST_BUILD_DIR"
    rm -rf "$BACKEND_BUILD_DIR"
    build \
        'x86_64' \
        'ON' \
        $TEST_BUILD_DIR \
        $ACR_MAX_WORKERS \
        $AIM_MAX_WORKERS
    ln -s "${TEST_BUILD_DIR}" "${BACKEND_BUILD_DIR}"
fi

# -- BUILD --
