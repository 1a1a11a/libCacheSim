#!/bin/bash 

SOURCE=$(readlink -f ${BASH_SOURCE[0]})
SCRIPT_DIR=$(dirname ${SOURCE})
CURR_DIR=$(pwd)

usage() {
    echo "Usage: $0 [options] [-- program_args]"
    echo "Options:"
    echo "  -h, --help    Show this help message"
    echo "  -c, --clean   Clean the build directory"
    echo "  --            Separator between script options and program arguments"
    echo ""
    echo "Example:"
    echo "  $0 -c -- ${SCRIPT_DIR}/../data/cloudPhysicsIO.oracleGeneral.bin oracleGeneral LRU,S3-FIFO 200M,1GB"
    echo "  note that the trace filepath is relative to current directory"
    exit 1
}

# Parse command line arguments
CLEAN=0
PROGRAM_ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -c|--clean)
            CLEAN=1
            shift
            ;;
        --)
            shift
            PROGRAM_ARGS=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

cd ${SCRIPT_DIR}/../

# Clean build directory if requested
if [ $CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf _build_dbg || true 2>/dev/null;
fi

# Create and enter build directory
mkdir -p _build_dbg
cd _build_dbg

# Configure and build with warning flags
echo "Configuring and building project with strict warnings..."
cmake -DCMAKE_BUILD_TYPE=Debug .. 
    #   -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -Wpedantic -Wformat=2 -Wformat-security -Wunused-variable -Wunused-parameter -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs" \
    #   -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror -Wpedantic -Wformat=2 -Wformat-security -Wunused-variable -Wunused-parameter -Wshadow -Wwrite-strings -Wredundant-decls -Wmissing-include-dirs" \
    #   ..

make -j

# Return to script directory
# cd ${DIR}
cd ${CURR_DIR}

if [ ${#PROGRAM_ARGS[@]} -ne 0 ]; then
    # Run the program with gdb and pass arguments
    echo "Starting debug session with arguments..."
    gdb -ex "set print thread-events off" -ex r --args ${SCRIPT_DIR}/../_build_dbg/bin/cachesim "${PROGRAM_ARGS[@]}"
else
    echo ''
    echo '########################################################'
    echo "debug build is at ${SCRIPT_DIR}/../_build_dbg/bin/cachesim"
    echo "you can debug cachesim by running: "
    echo "gdb -ex r --args PATH_TO_CACHESIM <trace_filepath> <trace_type> <cache_name> <cache_size>"
    echo "or you can provide cachesim arguments when running the debug script: "
    echo "bash scripts/debug.sh <trace_filepath> <trace_type> <cache_name> <cache_size>"
    echo '########################################################'
    echo ''
fi

