#!/bin/bash

# scripts/build_package_macos.sh 6.0.0.100 CrealityPrint Alpha 0

set -e
set -o pipefail
# Set defaults

if [ -z "$ARCH" ]; then
  ARCH="$(uname -m)"
  export ARCH
fi

if [ -z "$BUILD_CONFIG" ]; then
  export BUILD_CONFIG="Release"
fi

if [ -z "$BUILD_TARGET" ]; then
  export BUILD_TARGET="slicer"
fi

if [ -z "$SLICER_CMAKE_GENERATOR" ]; then
  export SLICER_CMAKE_GENERATOR="Xcode"
fi

if [ -z "$SLICER_BUILD_TARGET" ]; then
  export SLICER_BUILD_TARGET="ALL_BUILD"
fi

if [ -z "$DEPS_CMAKE_GENERATOR" ]; then
  export DEPS_CMAKE_GENERATOR="Unix Makefiles"
fi

if [ -z "$OSX_DEPLOYMENT_TARGET" ]; then
  export OSX_DEPLOYMENT_TARGET="11.3"
fi

VERSION_TAG_NAME=$1
APPNAME=$2
VERSION_EXTRA=$3
SLICER_HEADER=$4
if  [ -z "$SLICER_HEADER" ]; then
    export SLICER_HEADER=1
fi 
if [ -z "$VERSION_TAG_NAME" ]; then
    export VERSION_TAG_NAME="6.0.0"
fi

if [ -z "$APPNAME" ]; then
    export APPNAME="CrealityPrint"
fi

if [ -z "$VERSION_EXTRA" ]; then
    export APPNAME="Alpha"
fi

echo "Build params:"
echo " - ARCH: $ARCH"
echo " - BUILD_CONFIG: $BUILD_CONFIG"
echo " - BUILD_TARGET: $BUILD_TARGET"
echo " - CMAKE_GENERATOR: $SLICER_CMAKE_GENERATOR for Slicer, $DEPS_CMAKE_GENERATOR for deps"
echo " - OSX_DEPLOYMENT_TARGET: $OSX_DEPLOYMENT_TARGET"
echo " - SLICER_BUILD_TARGET=$SLICER_BUILD_TARGET"
echo " - BUILD_TARGET=$BUILD_TARGET"


PROJECT_DIR="$(pwd)" #"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_BUILD_DIR="$PROJECT_DIR/build_$ARCH"
DEPS_DIR="$PROJECT_DIR/deps"
DEPS_BUILD_DIR="$DEPS_DIR/build_$ARCH"
DEPS="$DEPS_BUILD_DIR/dep_$ARCH"

BUILD_CONFIG="Release"
echo " - PROJECT_BUILD_DIR=$PROJECT_BUILD_DIR"

# Fix for Multi-config generators
if [ "$SLICER_CMAKE_GENERATOR" == "Xcode" ]; then
    export BUILD_DIR_CONFIG_SUBDIR="/$BUILD_CONFIG"
else
    export BUILD_DIR_CONFIG_SUBDIR=""
fi

# 读取环境变量 MY_DIR
DEPS_PATH=$DEPS_ENV_DIR
if [ -z "${DEPS_PATH}" ]; then
    echo "env ${DEPS_PATH} is empty."
    export BUILD_TARGET="all"
else
    DEPS=$DEPS_PATH
    export BUILD_TARGET="slicer"
fi
echo "=====DEPS=====: $DEPS"

#DEPS_PATH="/Users/creality/Orca_work/c3d_6.0/C3DSlicer/deps/build_x86_64/dep_x86_64"


function build_deps() {
    echo "Building deps..."
    (
        set -x
        mkdir -p "$DEPS"
        cd "$DEPS_BUILD_DIR"
        if [ "1." != "$BUILD_ONLY". ]; then
            cmake .. \
                -G "${DEPS_CMAKE_GENERATOR}" \
                -DDESTDIR="$DEPS" \
                -DOPENSSL_ARCH="darwin64-${ARCH}-cc" \
                -DCMAKE_BUILD_TYPE="$BUILD_CONFIG" \
                -DCMAKE_OSX_ARCHITECTURES:STRING="${ARCH}" \
                -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}"
        fi
        cmake --build . --config "$BUILD_CONFIG" --target deps
    )
}

function pack_deps() {
    echo "Packing deps..."
    (
        set -x
        mkdir -p "$DEPS"
        cd "$DEPS_BUILD_DIR"
        tar -zcvf "dep_mac_${ARCH}_$(date +"%Y%m%d").tar.gz" "dep_$ARCH"
    )
}
function pack_slicer() {
    echo "Packing slicer"
    (
        cd "$PROJECT_BUILD_DIR"
        cmake --build . --target package --config $BUILD_CONFIG
    )
}
function build_slicer() {
    echo "Building slicer..."
    (
        echo " - SLICER_HEADER=$SLICER_HEADER"
        set -x
        mkdir -p "$PROJECT_BUILD_DIR"
        cd "$PROJECT_BUILD_DIR"
        if [ "1." != "$BUILD_ONLY". ]; then
            cmake .. \
                -G "${SLICER_CMAKE_GENERATOR}" \
                -DBBL_RELEASE_TO_PUBLIC=1 \
                -DGENERATE_ORCA_HEADER=$SLICER_HEADER \
                -DCMAKE_PREFIX_PATH="$DEPS/usr/local" \
                -DCMAKE_INSTALL_PREFIX="$PWD/CrealityPrint" \
                -DCMAKE_BUILD_TYPE="$BUILD_CONFIG" \
                -DCMAKE_MACOSX_RPATH=ON \
                -DCMAKE_INSTALL_RPATH="${DEPS}/usr/local" \
                -DCMAKE_MACOSX_BUNDLE=ON \
                -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
                -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
                -DPROCESS_NAME=$APPNAME \
                -DCREALITYPRINT_VERSION=$VERSION_TAG_NAME \
                -DPROJECT_VERSION_EXTRA=$VERSION_EXTRA
        fi
        cmake --build . --config "$BUILD_CONFIG" --target "$SLICER_BUILD_TARGET" || exit -2
    )

    echo "Verify localization with gettext..."
    (
        cd "$PROJECT_DIR"
        ./run_gettext.sh
    )

    # echo "Fix macOS app package..."
    # (
    #     cd "$PROJECT_BUILD_DIR"
    #     mkdir -p CrealityPrint
    #     cd CrealityPrint
    #     # remove previously built app
    #     # rm -rf ./CrealityPrint.app
    #     cp -pR  "../src$BUILD_DIR_CONFIG_SUBDIR/CrealityPrint.app" ./
    # )
}

case "${BUILD_TARGET}" in
    all)
        build_deps
        build_slicer
        pack_slicer
        ;;
    deps)
        build_deps
        ;;
    slicer)
        build_slicer
        pack_slicer
        ;;
    *)
        echo "Unknown target: $BUILD_TARGET. Available targets: deps, slicer, all."
        exit 1
        ;;
esac
