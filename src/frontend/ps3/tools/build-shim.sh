#!/bin/sh
# Builds libcrypto.so.1.1 next to this script: a thin shim that forwards the few
# OpenSSL 1.1 functions the ps3dev signing tools import (make_self,
# make_self_npdrm, package_finalize) to the system's libcrypto.so.3.
# Use it when those tools fail with "libcrypto.so.1.1: cannot open shared object file":
#
#     tools/build-shim.sh
#     LD_LIBRARY_PATH="$PWD/tools" make pkg
set -e
cd "$(dirname "$0")"
gcc -shared -fPIC -O2 -Wall -o libcrypto.so.1.1 libcrypto11_shim.c \
    -Wl,--version-script=libcrypto11.map -Wl,-soname,libcrypto.so.1.1 -ldl
echo "built $(pwd)/libcrypto.so.1.1"
