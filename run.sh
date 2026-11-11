#!/usr/bin/env bash
# ============================================================
# 一键构建（可选烧录）——沿用 Cmake_threadX_Template 的 run.sh
#
#   ./run.sh              仅编译，CMake 自动产出 .elf / .hex / .bin / .map
#   ./run.sh flash        编译后立即用 OpenOCD 烧录（openocd -f flash）
#   ./run.sh clean        清空 build 目录后重新编译
#   BUILD_TYPE=Release ./run.sh
#
# 并行度：默认 4。若在 Windows 上出现编译长时间卡住（某个 gcc 进程无进展），
# 这是 mingw 高并行度下偶发的进程派生死锁，与代码无关，降到 2 即可：
#   JOBS=2 ./run.sh
# ============================================================
set -e

cd "$(dirname "$0")"

# ---------- 工具链探测：PATH 里找不到时回退到本机安装目录 ----------
TOOLCHAIN_ROOT="/d/Embedded_Toolchain"
command -v arm-none-eabi-gcc >/dev/null 2>&1 || \
    export PATH="$TOOLCHAIN_ROOT/arm-none-eabi-gcc/15.3.rel2/bin:$PATH"
command -v cmake >/dev/null 2>&1 || \
    export PATH="$TOOLCHAIN_ROOT/cmake/4.4.5/bin:$PATH"
command -v ninja >/dev/null 2>&1 || \
    export PATH="$TOOLCHAIN_ROOT/ninja/1.13.2:$PATH"
command -v openocd >/dev/null 2>&1 || \
    export PATH="$TOOLCHAIN_ROOT/xpack-openocd/0.12.0-7/xpack-openocd-0.12.0-7/bin:$PATH"

# 产物名自动跟随目录名（CMakeLists.txt 同样按目录名命名工程）
ELF_NAME="$(basename "$(pwd)")"
BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
JOBS="${JOBS:-4}"

if [ "$1" = "clean" ]; then
    rm -rf "$BUILD_DIR"
    shift
fi

cmake -S . -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

# 兜底修正 compile_commands.json 的 MSYS 风格路径（供 clangd 使用）
if [ -f "$BUILD_DIR/compile_commands.json" ]; then
    sed -i "s|/c/|c:/|g; s|/d/|d:/|g; s|/e/|e:/|g; s|/f/|f:/|g;" "$BUILD_DIR/compile_commands.json"
fi

echo ""
echo "构建完成（$BUILD_TYPE）："
echo "  $BUILD_DIR/$ELF_NAME.elf"
echo "  $BUILD_DIR/$ELF_NAME.hex"
echo "  $BUILD_DIR/$ELF_NAME.bin"
echo "  $BUILD_DIR/$ELF_NAME.map"

if [ "$1" = "flash" ]; then
    echo ""
    echo "开始烧录 ..."
    openocd -f flash
fi
