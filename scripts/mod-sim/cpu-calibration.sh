#!/bin/bash
set -euo pipefail

NAM_LV2_COMMIT=ec06c7580a260935425f91f3b306c58326206175
NAM_LV2_URI=http://github.com/mikeoliphant/neural-amp-modeler-lv2
OCTOBASS_URI=https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass
WORK=/work/cpu
SOURCE=/src

mkdir -p "${WORK}/lv2" "${WORK}/models"
if [ ! -d "${WORK}/neural-amp-modeler-lv2" ]; then
    git clone --recursive https://github.com/mod-audio/neural-amp-modeler-lv2.git "${WORK}/neural-amp-modeler-lv2"
    git -C "${WORK}/neural-amp-modeler-lv2" checkout "${NAM_LV2_COMMIT}"
    git -C "${WORK}/neural-amp-modeler-lv2" submodule update --init --recursive
fi
cmake -S "${WORK}/neural-amp-modeler-lv2" -B "${WORK}/nam-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_UTILS=OFF -DBUILD_NAMCORE=ON -DBUILD_RTNEURAL=OFF -DWAVENET_FRAMES=128 -DBUFFER_PADDING=16 \
    -DSMART_BYPASS_ENABLED=OFF -DBUILD_INTERNAL_STATIC_WAVENET=ON -DBUILD_INTERNAL_STATIC_LSTM=ON \
    -DBUILD_STATIC_RTNEURAL=OFF -DWAVENET_MATH=FastMath -DLSTM_MATH=FastMath -DNAM_ENABLE_A2_FAST=ON \
    -DNAM_USE_INLINE_GEMM=ON -DDEFAULT_QUALITY_SCALE=0.49 -DCMAKE_CXX_FLAGS="-ffast-math -fno-finite-math-only" >/dev/null
cmake --build "${WORK}/nam-build" >/dev/null
rm -rf "${WORK}/lv2/neural_amp_modeler.lv2"
cp -r "$(find "${WORK}/nam-build" -maxdepth 2 -type d -name neural_amp_modeler.lv2 | head -1)" "${WORK}/lv2/"

cmake -S "${SOURCE}" -B "${WORK}/octobass-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_OCTOBIR=OFF -DBUILD_OCTOBIR_JUCE=OFF -DBUILD_OCTOBIR_VCV=OFF -DBUILD_OCTOBASS_JUCE=OFF \
    -DBUILD_OCTOBASS_LV2=ON -DOCTOBASS_NAM_A2_FAST=ON -DCMAKE_CXX_FLAGS="-ffast-math -fno-finite-math-only" >/dev/null
cmake --build "${WORK}/octobass-build" --target octobass-lv2-bundle >/dev/null
rm -rf "${WORK}/lv2/OctoBASS.lv2"
cp -r "${WORK}/octobass-build/plugins/octobass/lv2/OctoBASS.lv2" "${WORK}/lv2/"

g++ -O2 -std=c++17 "${SOURCE}/scripts/mod-sim/lv2_cpu_bench.cpp" -o "${WORK}/lv2-cpu-bench" \
    $(pkg-config --cflags --libs lilv-0)
python3 "${SOURCE}/scripts/mod-sim/make_timing_models.py" "${SOURCE}/tests/data/INPUT_VHD.nam" "${WORK}/models" >/dev/null

export LV2_PATH="${WORK}/lv2"
BENCH="${WORK}/lv2-cpu-bench --block 128 --seconds ${BENCH_SECONDS:-20}"
CAPTURE="${SOURCE}/tones/nam_captures/HM2-W OctoBASS distortion 0.nam"
IR="${SOURCE}/tones/impulse_responses/sm57 gray wolf 1.wav"
NAM1="${OCTOBASS_URI}#nam1Model"
NAM2="${OCTOBASS_URI}#nam2Model"
CAB="${OCTOBASS_URI}#cabinetIr"

run() {
    local label=$1
    shift
    printf "%-44s " "${label}"
    ${BENCH} "$@" 2>/dev/null | tail -1
}

echo "== MOD Neural Amp Modeler (calibration)"
run "nam nano" --uri "${NAM_LV2_URI}" --model "${WORK}/models/timing-nano.nam"
run "nam feather" --uri "${NAM_LV2_URI}" --model "${WORK}/models/timing-feather.nam"
run "nam A2 capture, quality 49%" --uri "${NAM_LV2_URI}" --model "${CAPTURE}"

echo "== OctoBASS"
run "no files" --uri "${OCTOBASS_URI}"
run "IR only" --uri "${OCTOBASS_URI}" --file "${CAB}=${IR}"
run "1 slot A2 slim + IR" --uri "${OCTOBASS_URI}" --file "${NAM1}=${CAPTURE}" --file "${CAB}=${IR}"
run "2 slots A2 slim + IR" --uri "${OCTOBASS_URI}" --file "${NAM1}=${CAPTURE}" --file "${NAM2}=${CAPTURE}" --file "${CAB}=${IR}"
run "1 slot nano + IR" --uri "${OCTOBASS_URI}" --file "${NAM1}=${WORK}/models/timing-nano.nam" --file "${CAB}=${IR}"
