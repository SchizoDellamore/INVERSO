#!/usr/bin/env bash
# INVERSO quality gate. Nothing is pushed unless this passes (see .githooks/pre-push).
#
#   ./check.sh          build everything, then: DSP tests, DSP tests under ASan/UBSan,
#                       JUCE host smokes, VST3 conformance of every plugin
#   ./check.sh --deep   also builds every plugin with ASan/UBSan and runs the VST3
#                       conformance on those builds (memory errors, undefined behaviour)
#
# JUCE: $JUCE_ROOT, or the JUCE the existing build used, or ../JUCE.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"
deep=0; [ "${1:-}" = "--deep" ] && deep=1
jobs="$(nproc 2>/dev/null || echo 4)"
juce="${JUCE_ROOT:-}"
[ -z "$juce" ] && [ -f build-linux/CMakeCache.txt ] && juce="$(grep '^JUCE_ROOT:' build-linux/CMakeCache.txt | cut -d= -f2)"
[ -z "$juce" ] && juce="$here/../JUCE"
[ -f "$juce/CMakeLists.txt" ] || { echo "JUCE not found (set JUCE_ROOT)"; exit 2; }
# GUI tests need a display; without one, a virtual one (Xvfb) with a window manager, as on a desktop
# (editors opened on screen need it). Missing tools are installed once.
if [ -z "${DISPLAY:-}" ]; then
 for tool in xvfb-run:xvfb openbox:openbox; do
  command -v "${tool%%:*}" >/dev/null || { apt-get install -y --no-install-recommends "${tool##*:}" >/dev/null 2>&1 || { apt-get update >/dev/null 2>&1 && apt-get install -y --no-install-recommends "${tool##*:}" >/dev/null 2>&1; }; }
 done
fi
run_gui() {
 if [ -n "${DISPLAY:-}" ]; then "$@"; return; fi
 xvfb-run -a bash -c 'openbox >/dev/null 2>&1 & wm=$!; sleep 1; "$@"; rc=$?; kill $wm 2>/dev/null; exit $rc' run_gui "$@"
}

results=(); failed=0
step() { # name, command...
 local name="$1"; shift
 local log="$here/build-check-logs/$(echo "$name" | tr ' /' '__').log"
 mkdir -p "$here/build-check-logs"
 local t0=$SECONDS
 if "$@" >"$log" 2>&1; then results+=("  ok    $name ($((SECONDS-t0)) s)")
 else results+=("  FAIL  $name ($((SECONDS-t0)) s) - see $log"); failed=1
  echo "---- $name failed; last lines of $log:"; tail -25 "$log"; fi
}
plugins=(InversoAbismo InversoAbismoSampler InversoAbismoDrums InversoRelicario InversoEnki InversoClear InversoTide InversoNeon InversoHalo InversoMaster InversoKick InversoSnare InversoToms InversoHats InversoCymbals InversoOverheads)
vst3_targets() { for p in "${plugins[@]}"; do printf '%s_VST3 ' "$p"; done; }

# 0. nothing temporary left in the sources
step "no leftovers" bash -c '! grep -rnE "DIAG|XXX|FIXME|getenv\(\"INVERSO_DIAG" Source Tests CMakeLists.txt --include=*.h --include=*.cpp --include=CMakeLists.txt'

# 1. portable DSP tests, and the same tests under AddressSanitizer + UndefinedBehaviorSanitizer
step "DSP build" bash -c "cmake -S . -B build-dsp -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-dsp -j$jobs"
step "DSP tests" ctest --test-dir build-dsp --output-on-failure -j"$jobs"
step "DSP build (ASan/UBSan)" bash -c "cmake -S . -B build-dsp-asan -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -DNEON_SANITIZED' >/dev/null && cmake --build build-dsp-asan -j$jobs"
step "DSP tests (ASan/UBSan)" ctest --test-dir build-dsp-asan --output-on-failure -j"$jobs"

# 2. the plugins, their JUCE host smokes and the VST3 conformance of every bundle
step "plugin build" bash -c "cmake -S . -B build-linux -DJUCE_ROOT='$juce' -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-linux -j$jobs"
step "host smokes" run_gui ctest --test-dir build-linux --output-on-failure -R host_smoke
step "VST3 conformance" run_gui ctest --test-dir build-linux --output-on-failure -R vst3_conformance -j2

# 3. --deep: the plugins themselves under ASan/UBSan, through the same conformance
if [ $deep = 1 ]; then
 step "plugin build (ASan/UBSan)" bash -c "cmake -S . -B build-asan -DJUCE_ROOT='$juce' -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS_RELWITHDEBINFO='-O1 -g' -DCMAKE_C_FLAGS_RELWITHDEBINFO='-O1 -g' -DINVERSO_LTO=OFF \
   -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined' \
   -DCMAKE_C_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
   -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined \
   -DCMAKE_MODULE_LINKER_FLAGS=-fsanitize=address,undefined >/dev/null && cmake --build build-asan -j$jobs --target $(vst3_targets) InversoVst3Conformance"
 step "VST3 conformance (ASan/UBSan)" bash -c "export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1; \
   $(declare -f run_gui); run_gui ctest --test-dir build-asan --output-on-failure -R vst3_conformance"
fi

echo; echo "INVERSO quality gate$([ $deep = 1 ] && echo ' (deep)'):"
printf '%s\n' "${results[@]}"
[ $failed = 0 ] && echo "ALL PASSED" || echo "NOT READY: fix every FAIL above before pushing"
if [ $failed = 0 ] && [ -z "$(git -C "$here/.." status --porcelain -- Abismo .github 2>/dev/null)" ]; then
 git -C "$here" rev-parse HEAD > "$here/build-check-logs/passed-commit"
fi
exit $failed
