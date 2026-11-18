#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
    CC="${CC:-cc}"
    CXX="${CXX:-c++}"
    BUILD="$(mktemp -d "${TMPDIR:-/tmp}/hyperborea-native.XXXXXX")"
    trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
    CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror"
    CXXFLAGS="-std=c++17 -Wall -Wextra -Wpedantic -Werror"
    mkdir -p "$BUILD/include/hb"
    for header in "$ROOT"/Firmware/SRC/*.h "$ROOT/Host and SRC"/*.hpp; do ln -s "$header" "$BUILD/include/hb/"; done
    ln -s "$ROOT/Third Party" "$BUILD/vendor"
    INC="-I$BUILD/include -I$BUILD/vendor"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/Tests/test_ads1299.c" \
        "$ROOT/Firmware/SRC/ads1299.c" "$ROOT/Firmware/SRC/acquisition.c" \
        "$ROOT/Firmware/SRC/protocol.c" -o "$BUILD/test_ads1299"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/Tests/test_dual_ads1299.c" \
        "$ROOT/Firmware/SRC/ads1299.c" "$ROOT/Firmware/SRC/dual_ads1299.c" \
        "$ROOT/Firmware/SRC/dual_acquisition.c" "$ROOT/Firmware/SRC/protocol.c" \
        -o "$BUILD/test_dual_ads1299"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/Tests/test_queue.c" \
        "$ROOT/Firmware/SRC/packet_queue.c" -o "$BUILD/test_queue"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/Tests/test_w25q128.c" \
        "$ROOT/Firmware/SRC/w25q128.c" -o "$BUILD/test_w25q128"
    "$CC" $CFLAGS -UNDEBUG $INC "$ROOT/Tests/test_bmi270.c" \
        "$ROOT/Firmware/SRC/bmi270_driver.c" "$ROOT/Third Party/bmi2.c" \
        "$ROOT/Third Party/bmi270.c" -o "$BUILD/test_bmi270"
    for source in ads1299 dual_ads1299 dual_acquisition protocol acquisition bmi270_driver; do
        "$CC" $CFLAGS $INC -c "$ROOT/Firmware/SRC/$source.c" -o "$BUILD/$source.o"
    done
    "$CXX" $CXXFLAGS -UNDEBUG $INC -I"$BUILD/include" \
        "$ROOT/Tests/test_packet.cpp" "$ROOT/Host and SRC/packet.cpp" \
        "$BUILD/ads1299.o" "$BUILD/dual_ads1299.o" "$BUILD/dual_acquisition.o" \
        "$BUILD/protocol.o" "$BUILD/acquisition.o" -o "$BUILD/test_packet"
    "$CXX" $CXXFLAGS -UNDEBUG -I"$BUILD/include" \
        "$ROOT/Tests/test_dsp.cpp" "$ROOT/Host and SRC/dsp.cpp" -o "$BUILD/test_dsp"
    "$CXX" $CXXFLAGS -I"$BUILD/include" -I"$BUILD/include" \
        "$ROOT/Host and SRC/analyze.cpp" "$ROOT/Host and SRC/packet.cpp" "$ROOT/Host and SRC/dsp.cpp" \
        "$BUILD/protocol.o" -o "$BUILD/hb_analyze"
    "$CXX" $CXXFLAGS $INC -I"$BUILD/include" \
        "$ROOT/Host and SRC/decode.cpp" "$ROOT/Host and SRC/packet.cpp" \
        "$BUILD/protocol.o" -o "$BUILD/hb_decode"
    for test in test_ads1299 test_dual_ads1299 test_packet test_queue test_w25q128 test_bmi270 test_dsp; do
        "$BUILD/$test"
    done
    echo "Strict-warning C11/C++17 fallback build and tests passed (CMake unavailable)."
