#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if command -v cmake >/dev/null 2>&1; then
    cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$ROOT/build" --parallel
    ctest --test-dir "$ROOT/build" --output-on-failure
else
    CC="${CC:-cc}"
    CXX="${CXX:-c++}"
    BUILD="$(mktemp -d "${TMPDIR:-/tmp}/hyperborea-native.XXXXXX")"
    trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
    CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror"
    CXXFLAGS="-std=c++17 -Wall -Wextra -Wpedantic -Werror"
    INC="-I$ROOT/firmware/include -I$ROOT/third_party/bmi270"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/tests/test_ads1299.c" \
        "$ROOT/firmware/src/ads1299.c" "$ROOT/firmware/src/acquisition.c" \
        "$ROOT/firmware/src/protocol.c" -o "$BUILD/test_ads1299"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/tests/test_dual_ads1299.c" \
        "$ROOT/firmware/src/ads1299.c" "$ROOT/firmware/src/dual_ads1299.c" \
        "$ROOT/firmware/src/dual_acquisition.c" "$ROOT/firmware/src/protocol.c" \
        -o "$BUILD/test_dual_ads1299"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/tests/test_queue.c" \
        "$ROOT/firmware/src/packet_queue.c" -o "$BUILD/test_queue"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/tests/test_w25q128.c" \
        "$ROOT/firmware/src/w25q128.c" -o "$BUILD/test_w25q128"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/tests/test_bmi270.c" \
        "$ROOT/firmware/src/bmi270_driver.c" "$ROOT/third_party/bmi270/bmi2.c" \
        "$ROOT/third_party/bmi270/bmi270.c" -o "$BUILD/test_bmi270"
    for source in ads1299 dual_ads1299 dual_acquisition protocol acquisition bmi270_driver; do
        "$CC" $CFLAGS $INC -c "$ROOT/firmware/src/$source.c" -o "$BUILD/$source.o"
    done
    "$CXX" $CXXFLAGS -UNDEBUG $INC -I"$ROOT/host/include" \
        "$ROOT/tests/test_packet.cpp" "$ROOT/host/src/packet.cpp" \
        "$BUILD/ads1299.o" "$BUILD/dual_ads1299.o" "$BUILD/dual_acquisition.o" \
        "$BUILD/protocol.o" "$BUILD/acquisition.o" -o "$BUILD/test_packet"
    "$CXX" $CXXFLAGS -UNDEBUG -I"$ROOT/host/include" \
        "$ROOT/tests/test_dsp.cpp" "$ROOT/host/src/dsp.cpp" -o "$BUILD/test_dsp"
    "$CXX" $CXXFLAGS -I"$ROOT/host/include" -I"$ROOT/firmware/include" \
        "$ROOT/host/src/analyze.cpp" "$ROOT/host/src/packet.cpp" "$ROOT/host/src/dsp.cpp" \
        "$BUILD/protocol.o" -o "$BUILD/hb_analyze"
    "$CXX" $CXXFLAGS $INC -I"$ROOT/host/include" \
        "$ROOT/host/src/decode.cpp" "$ROOT/host/src/packet.cpp" \
        "$BUILD/protocol.o" -o "$BUILD/hb_decode"
    for test in test_ads1299 test_dual_ads1299 test_packet test_queue test_w25q128 test_bmi270 test_dsp; do
        "$BUILD/$test"
    done
    echo "Strict-warning C11/C++17 fallback build and tests passed (CMake unavailable)."
fi
python3 "$ROOT/tools/verify_rev27_interfaces.py"
