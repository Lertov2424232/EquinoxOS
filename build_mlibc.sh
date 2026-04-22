#!/bin/bash
set -e

# Configuration
MLIBC_REPO="https://github.com/managarm/mlibc.git"
MLIBC_DIR="mlibc"
SYSDEPS_SRC="sdk/mlibc/sysdeps/equinox"
BUILD_DIR="mlibc_build"
CROSS_FILE="equinox-cross.txt"
INSTALL_PREFIX="$(pwd)/sdk"

echo "=== EquinoxOS mlibc Build Script ==="

# 1. Clone mlibc if missing
if [ ! -d "$MLIBC_DIR" ]; then
    echo "[*] Cloning mlibc repository..."
    git clone "$MLIBC_REPO" "$MLIBC_DIR"
else
    echo "[*] mlibc repository already exists."
fi

# 2. Inject sysdeps
echo "[*] Injecting Equinox sysdeps..."

# Create sysdeps source directory if missing (injector)
# Always regenerate to ensure correct meson.build
if [ -d "$SYSDEPS_SRC" ]; then
    rm -rf "$SYSDEPS_SRC"
fi
echo "[*] Creating sysdeps stub files..."
mkdir -p "$SYSDEPS_SRC/include"
mkdir -p "$SYSDEPS_SRC/src"

# Create minimal meson.build for equinox sysdeps
cat > "$SYSDEPS_SRC/meson.build" <<'SYSDEPS_MESON'
# EquinoxOS sysdeps meson.build

sysdep_supported_options = {
    'posix': false,
    'linux': false,
    'glibc': false,
    'bsd': false,
}

rtld_sources += files(
    'src/sysdeps.cpp',
)
rtld_include_dirs += include_directories('include')

libc_sources += files(
    'src/sysdeps.cpp',
)
libc_include_dirs += include_directories('include')
SYSDEPS_MESON

# Create minimal sysdeps.cpp stub
cat > "$SYSDEPS_SRC/src/sysdeps.cpp" <<'SYSDEPS_CPP'
#include <bits/ensure.h>
#include <mlibc/debug.hpp>
#include <mlibc/all-sysdeps.hpp>

namespace mlibc {
    void sys_libc_log(const char *message) {
        // TODO: Implement kernel logging syscall
    }

    void sys_libc_panic() {
        // TODO: Implement kernel panic
        __builtin_unreachable();
    }

    int sys_futex_tid() {
        return 1;
    }
}
SYSDEPS_CPP

# Copy all abi-bits headers from demo sysdeps as a starting point
mkdir -p "$SYSDEPS_SRC/include/abi-bits"
if [ -d "$MLIBC_DIR/sysdeps/demo/include/abi-bits" ]; then
    cp -r "$MLIBC_DIR/sysdeps/demo/include/abi-bits/"* "$SYSDEPS_SRC/include/abi-bits/"
else
    # Fallback: create minimal abi-bits headers if demo not available
    cat > "$SYSDEPS_SRC/include/abi-bits/limits.h" <<'LIMITS_H'
#ifndef _ABIBITS_LIMITS_H
#define _ABIBITS_LIMITS_H
#define __MLIBC_IOV_MAX 1024
#define __MLIBC_LOGIN_NAME_MAX 256
#define __MLIBC_HOST_NAME_MAX 64
#define __MLIBC_NAME_MAX 255
#define __MLIBC_OPEN_MAX 256
#endif
LIMITS_H
fi

# Create mlibc/sysdeps.hpp - required header defining sysdep tags
mkdir -p "$SYSDEPS_SRC/include/mlibc"
cat > "$SYSDEPS_SRC/include/mlibc/sysdeps.hpp" <<'SYSDEPS_HPP'
#pragma once

#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct EquinoxSysdepTags :
    LibcPanic,
    LibcLog,
    Isatty,
    Write,
    TcbSet,
    AnonAllocate,
    AnonFree,
    Seek,
    Exit,
    Close,
    FutexWake,
    FutexWait,
    Read,
    Open,
    VmMap,
    VmUnmap,
    ClockGet
{};

template<typename Tag>
using Sysdeps = SysdepOf<EquinoxSysdepTags, Tag>;

} // namespace mlibc
SYSDEPS_HPP

echo "[*] Stub files created successfully."

mkdir -p "$MLIBC_DIR/sysdeps/equinox"
cp -r $SYSDEPS_SRC/* "$MLIBC_DIR/sysdeps/equinox/"

# 3. Create Meson cross-file
echo "[*] Creating Meson cross-file ($CROSS_FILE)..."
cat > $CROSS_FILE <<EOF
[binaries]
c = 'x86_64-elf-gcc'
cpp = 'x86_64-elf-g++'
ar = 'x86_64-elf-ar'
strip = 'x86_64-elf-strip'

[host_machine]
system = 'equinox'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[built-in options]
c_args = ['-ffreestanding', '-D__clang__', '-D_GNU_SOURCE']
cpp_args = ['-ffreestanding', '-fno-exceptions', '-fno-rtti', '-D__clang__', '-D_GNU_SOURCE']
EOF

# 4. Patch mlibc/meson.build to accept 'equinox' system
echo "[*] Registering 'equinox' in mlibc/meson.build..."
if ! grep -q "'equinox'" "$MLIBC_DIR/meson.build"; then
    # We insert our elif block before the demo-sysdeps anchor
    sed -i "/# ANCHOR: demo-sysdeps/i elif host_machine.system() == 'equinox'\n    rtld_include_dirs += include_directories('sysdeps/equinox/include')\n    libc_include_dirs += include_directories('sysdeps/equinox/include')\n    subdir('sysdeps/equinox')\n" "$MLIBC_DIR/meson.build"
fi

# 5. Build and Install
echo "[*] Setting up Meson build directory..."
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi

# Note: Removed -Dsysdeps=equinox as it is determined by host_machine.system
meson setup "$BUILD_DIR" "$MLIBC_DIR" \
    --cross-file "$CROSS_FILE" \
    -Dheaders_only=false \
    -Ddefault_library=static \
    --prefix="$INSTALL_PREFIX"

echo "[*] Compiling mlibc..."
ninja -C "$BUILD_DIR"

echo "[*] Installing mlibc to $INSTALL_PREFIX..."
ninja -C "$BUILD_DIR" install

echo "=== Build Complete! ==="
echo "mlibc is now installed in sdk/lib and sdk/include"
