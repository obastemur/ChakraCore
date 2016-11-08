#!/bin/bash
#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

SAFE_RUN() {
    local SF_RETURN_VALUE=$($1 2>&1)

    if [[ $? != 0 ]]; then
        >&2 echo $SF_RETURN_VALUE
        exit 1
    fi
    echo $SF_RETURN_VALUE
}

ERROR_EXIT() {
    if [[ $? != 0 ]]; then
        echo $($1 2>&1)
        exit 1;
    fi
}

PRINT_USAGE() {
    echo ""
    echo "[ChakraCore Build Script Help]"
    echo ""
    echo "build.sh [options]"
    echo ""
    echo "options:"
    echo "  --arch=[*]           Set target arch (x86)"
    echo "      --cxx=PATH       Path to Clang++ (see example below)"
    echo "      --cc=PATH        Path to Clang   (see example below)"
    echo "  -d, --debug          Debug build (by default Release build)"
    echo "      --embed-icu      Download and embed ICU-57 statically"
    echo "  -h, --help           Show help"
    echo "      --icu=PATH       Path to ICU include folder (see example below)"
    echo "  -j [N], --jobs[=N]   Multicore build, allow N jobs at once"
    echo "  -n, --ninja          Build with ninja instead of make"
    echo "      --no-icu         Compile without unicode/icu support"
    echo "      --no-jit         Disable JIT"
    echo "      --xcode          Generate XCode project"
    echo "  -t, --test-build     Test build (by default Release build)"
    echo "      --static         Build as static library (by default shared library)"
    echo "      --sanitize=CHECKS Build with clang -fsanitize checks,"
    echo "                       e.g. undefined,signed-integer-overflow"
    echo "  -v, --verbose        Display verbose output including all options"
    echo "      --create-deb=V   Create .deb package with given V version"
    echo "      --without=FEATURE,FEATURE,..."
    echo "                       Disable FEATUREs from JSRT experimental"
    echo "                       features."
    echo ""
    echo "example:"
    echo "  ./build.sh --cxx=/path/to/clang++ --cc=/path/to/clang -j"
    echo "with icu:"
    echo "  ./build.sh --icu=/usr/local/Cellar/icu4c/version/include/"
    echo ""
}

CHAKRACORE_DIR=`dirname $0`
_CXX=""
_CC=""
VERBOSE=""
BUILD_TYPE="Release"
CMAKE_GEN=
MAKE=make
MULTICORE_BUILD=""
NO_JIT=
ICU_PATH="-DICU_SETTINGS_RESET=1"
STATIC_LIBRARY="-DSHARED_LIBRARY_SH=1"
SANITIZE=
WITHOUT_FEATURES=""
CREATE_DEB=0
ARCH="-DCC_TARGETS_AMD64_SH=1"
OS_LINUX=0
OS_APT_GET=0
OS_UNIX=0

if [ -f "/proc/version" ]; then
    OS_LINUX=1
    PROC_INFO=$(cat /proc/version)
    if [[ $PROC_INFO =~ 'Ubuntu' || $PROC_INFO =~ 'Debian'
       || $PROC_INFO =~ 'Linaro' ]]; then
        OS_APT_GET=1
    fi
else
    OS_UNIX=1
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
    --arch=*)
        ARCH=$1
        ARCH="${ARCH:7}"
        ;;

    --cxx=*)
        _CXX=$1
        _CXX=${_CXX:6}
        ;;

    --cc=*)
        _CC=$1
        _CC=${_CC:5}
        ;;

    -h | --help)
        PRINT_USAGE
        exit
        ;;

    -v | --verbose)
        _VERBOSE="V=1"
        ;;

    -d | --debug)
        BUILD_TYPE="Debug"
        ;;

    --embed-icu)
        if [ ! -d "${CHAKRACORE_DIR}/deps/icu/source/output" ]; then
            ICU_URL="http://source.icu-project.org/repos/icu/icu/tags/release-57-1"
            echo -e "\n----------------------------------------------------------------"
            echo -e "\nThis script will download ICU-LIB from\n${ICU_URL}\n"
            echo "It is licensed to you by its publisher, not Microsoft."
            echo "Microsoft is not responsible for the software."
            echo "Your installation and use of ICU-LIB is subject to the publisher’s terms available here:"
            echo -e "http://www.unicode.org/copyright.html#License\n"
            echo -e "----------------------------------------------------------------\n"
            echo "If you don't agree, press Ctrl+C to terminate"
            read -t 10 -p "Hit ENTER to continue (or wait 10 seconds)"
            SAFE_RUN `mkdir -p ${CHAKRACORE_DIR}/deps/`
            cd "${CHAKRACORE_DIR}/deps/";
            ABS_DIR=`pwd`
            if [ ! -d "${ABS_DIR}/icu/" ]; then
                echo "Downloading ICU ${ICU_URL}"
                if [ ! -f "/usr/bin/svn" ]; then
                    echo -e "\nYou should install 'svn' client in order to use this feature"
                    if [ $OS_APT_GET == 1 ]; then
                        echo "tip: Try 'sudo apt-get install subversion'"
                    fi
                    exit 1
                fi
                svn export -q $ICU_URL icu
                ERROR_EXIT "rm -rf ${ABS_DIR}/icu/"
            fi

            cd "${ABS_DIR}/icu/source";./configure --with-data-packaging=static\
                    --prefix="${ABS_DIR}/icu/source/output/"\
                    --enable-static --disable-shared --with-library-bits=64\
                    --disable-icuio --disable-layout\
                    CXXFLAGS="-fPIC" CFLAGS="-fPIC"

            ERROR_EXIT "rm -rf ${ABS_DIR}/icu/source/output/"
            make STATICCFLAGS="-fPIC" STATICCXXFLAGS="-fPIC" STATICCPPFLAGS="-DPIC" install
            ERROR_EXIT "rm -rf ${ABS_DIR}/icu/source/output/"
            cd "${ABS_DIR}/../"
        fi
        ICU_PATH="-DCC_EMBED_ICU_SH=1"
        ;;


    -t | --test-build)
        BUILD_TYPE="Test"
        ;;

    -j | --jobs)
        if [[ "$1" == "-j" && "$2" =~ ^[^-] ]]; then
            MULTICORE_BUILD="-j $2"
            shift
        else
            MULTICORE_BUILD="-j $(nproc)"
        fi
        ;;

    -j=* | --jobs=*)            # -j=N syntax used in CI
        MULTICORE_BUILD=$1
        if [[ "$1" =~ ^-j= ]]; then
            MULTICORE_BUILD="-j ${MULTICORE_BUILD:3}"
        else
            MULTICORE_BUILD="-j ${MULTICORE_BUILD:7}"
        fi
        ;;

    --icu=*)
        ICU_PATH=$1
        ICU_PATH="-DICU_INCLUDE_PATH_SH=${ICU_PATH:6}"
        ;;

    -n | --ninja)
        CMAKE_GEN="-G Ninja"
        MAKE=ninja
        ;;

    --no-icu)
        ICU_PATH="-DNO_ICU_PATH_GIVEN_SH=1"
        ;;

    --no-jit)
        NO_JIT="-DNO_JIT_SH=1"
        ;;

    --xcode)
        CMAKE_GEN="-G Xcode -DCC_XCODE_PROJECT=1"
        MAKE=0
        ;;

    --create-deb=*)
        CREATE_DEB=$1
        CREATE_DEB="${CREATE_DEB:13}"
        ;;

    --static)
        STATIC_LIBRARY="-DSTATIC_LIBRARY_SH=1"
        ;;

    --sanitize=*)
        SANITIZE=$1
        SANITIZE=${SANITIZE:11}    # value after --sanitize=
        SANITIZE="-DCLANG_SANITIZE_SH=${SANITIZE}"
        ;;

    --without=*)
        FEATURES=$1
        FEATURES=${FEATURES:10}    # value after --without=
        for x in ${FEATURES//,/ }  # replace comma with space then split
        do
            if [[ "$WITHOUT_FEATURES" == "" ]]; then
                WITHOUT_FEATURES="-DWITHOUT_FEATURES_SH="
            else
                WITHOUT_FEATURES="$WITHOUT_FEATURES;"
            fi
            WITHOUT_FEATURES="${WITHOUT_FEATURES}-DCOMPILE_DISABLE_${x}=1"
        done
        ;;

    *)
        echo "Unknown option $1"
        PRINT_USAGE
        exit -1
        ;;
    esac

    shift
done

if [[ ${#_VERBOSE} > 0 ]]; then
    # echo options back to the user
    echo "Printing command line options back to the user:"
    echo "_CXX=${_CXX}"
    echo "_CC=${_CC}"
    echo "BUILD_TYPE=${BUILD_TYPE}"
    echo "MULTICORE_BUILD=${MULTICORE_BUILD}"
    echo "ICU_PATH=${ICU_PATH}"
    echo "CMAKE_GEN=${CMAKE_GEN}"
    echo "MAKE=${MAKE} $_VERBOSE"
    echo ""
fi

CLANG_PATH=
if [[ ${#_CXX} > 0 || ${#_CC} > 0 ]]; then
    if [[ ${#_CXX} == 0 || ${#_CC} == 0 ]]; then
        echo "ERROR: '-cxx' and '-cc' options must be used together."
        exit 1
    fi
    echo "Custom CXX ${_CXX}"
    echo "Custom CC  ${_CC}"

    if [[ ! -f $_CXX || ! -f $_CC ]]; then
        echo "ERROR: Custom compiler not found on given path"
        exit 1
    fi
    CLANG_PATH=$_CXX
else
    RET_VAL=$(SAFE_RUN 'c++ --version')
    if [[ ! $RET_VAL =~ "clang" ]]; then
        echo "Searching for Clang..."
        if [[ -f /usr/bin/clang++ ]]; then
            echo "Clang++ found at /usr/bin/clang++"
            _CXX=/usr/bin/clang++
            _CC=/usr/bin/clang
            CLANG_PATH=$_CXX
        else
            echo "ERROR: clang++ not found at /usr/bin/clang++"
            echo ""
            echo "You could use clang++ from a custom location."
            PRINT_USAGE
            exit 1
        fi
    else
        CLANG_PATH=c++
    fi
fi

# check clang version (min required 3.7)
VERSION=$($CLANG_PATH --version | grep "version [0-9]*\.[0-9]*" --o -i | grep "[0-9]\.[0-9]*" --o)
VERSION=${VERSION/./}

if [[ $VERSION -lt 37 ]]; then
    echo "ERROR: Minimum required Clang version is 3.7"
    exit 1
fi

CC_PREFIX=""
if [[ ${#_CXX} > 0 ]]; then
    CC_PREFIX="-DCMAKE_CXX_COMPILER=$_CXX -DCMAKE_C_COMPILER=$_CC"
fi

build_directory="$CHAKRACORE_DIR/BuildLinux/${BUILD_TYPE:0}"
if [ ! -d "$build_directory" ]; then
    SAFE_RUN `mkdir -p $build_directory`
fi

pushd $build_directory > /dev/null

if [ $ARCH = "x86" ]; then
    ARCH="-DCC_TARGETS_X86_SH=1"
    echo "Compile Target : x86"
else
    echo "Compile Target : amd64"
fi

echo Generating $BUILD_TYPE makefiles
cmake $CMAKE_GEN $CC_PREFIX $ICU_PATH $STATIC_LIBRARY $ARCH \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE $SANITIZE $NO_JIT $WITHOUT_FEATURES ../..

_RET=$?
if [[ $? == 0 ]]; then
    if [[ $MAKE != 0 ]]; then
        $MAKE $MULTICORE_BUILD $_VERBOSE 2>&1 | tee build.log
        _RET=${PIPESTATUS[0]}
    else
        echo "Visit given folder above for xcode project file ----^"
    fi
fi

if [[ $_RET != 0 ]]; then
    echo "See error details above. Exit code was $_RET"
else
    if [[ $CREATE_DEB != 0 ]]; then
        DEB_FOLDER=`realpath .`
        DEB_FOLDER="${DEB_FOLDER}/chakracore_${CREATE_DEB}"

        mkdir -p $DEB_FOLDER/usr/local/bin
        mkdir -p $DEB_FOLDER/DEBIAN
        cp $DEB_FOLDER/../ch $DEB_FOLDER/usr/local/bin/
        if [[ $STATIC_LIBRARY == "-DSHARED_LIBRARY_SH=1" ]]; then
            cp $DEB_FOLDER/../*.so $DEB_FOLDER/usr/local/bin/
        fi
        echo -e "Package: ChakraCore"\
            "\nVersion: ${CREATE_DEB}"\
            "\nSection: base"\
            "\nPriority: optional"\
            "\nArchitecture: amd64"\
            "\nDepends: libc6 (>= 2.19), uuid-dev (>> 0), libunwind-dev (>> 0), libicu-dev (>> 0)"\
            "\nMaintainer: ChakraCore <chakracore@microsoft.com>"\
            "\nDescription: Chakra Core"\
            "\n Open source Core of Chakra Javascript Engine"\
            > $DEB_FOLDER/DEBIAN/control

        dpkg-deb --build $DEB_FOLDER
        _RET=$?
        if [[ $_RET == 0 ]]; then
            echo ".deb package is available under $build_directory"
        fi
    fi
fi

popd > /dev/null
exit $_RET
