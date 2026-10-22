#!/bin/bash
set -euo pipefail

SOURCE_DIR=/src
BUILD_DIR=/work/build
DATA_DIR=/work/data
LV2_DIR=${DATA_DIR}/lv2
USER_FILES_DIR=${DATA_DIR}/user-files

echo "[mod-sim] Building OctoBASS LV2 from ${SOURCE_DIR}"
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_OCTOBIR=OFF -DBUILD_OCTOBIR_JUCE=OFF -DBUILD_OCTOBIR_VCV=OFF \
    -DBUILD_OCTOBASS_JUCE=OFF -DBUILD_OCTOBASS_LV2=ON -DOCTOBASS_NAM_A2_FAST=ON >/dev/null
cmake --build "${BUILD_DIR}" --target octobass-lv2-bundle

mkdir -p "${LV2_DIR}" "${USER_FILES_DIR}/NAM Models" "${USER_FILES_DIR}/Speaker Cabinets IRs"
rm -rf "${LV2_DIR}/OctoBASS.lv2"
cp -r "${BUILD_DIR}/plugins/octobass/lv2/OctoBASS.lv2" "${LV2_DIR}/"
cp "${SOURCE_DIR}"/tones/nam_captures/*.nam "${USER_FILES_DIR}/NAM Models/"
cp "${SOURCE_DIR}"/tones/impulse_responses/*.wav "${USER_FILES_DIR}/Speaker Cabinets IRs/"
echo "[mod-sim] Installed ${LV2_DIR}/OctoBASS.lv2 and bundled tones"

export LV2_PATH="${LV2_DIR}:/usr/lib/lv2"

jackd --no-realtime -d dummy -r 48000 -p "${MOD_SIM_BUFFER_SIZE:-128}" &
jack_wait -w -t 10 >/dev/null
echo "[mod-sim] JACK dummy driver running at 48 kHz"

/opt/mod-host/mod-host -n -p 5555 -f 5556 &
sleep 1
echo "[mod-sim] mod-host listening on 5555"

cd /opt/mod-ui
export MOD_DEV_HMI=1
export MOD_DEV_HOST=0
export MOD_DEV_ENVIRONMENT=0
export MOD_DEVICE_WEBSERVER_PORT=8888
export MOD_DEVICE_HOST_PORT=5555
export MOD_DATA_DIR="${DATA_DIR}"
export MOD_USER_FILES_DIR="${USER_FILES_DIR}"
export MOD_HTML_DIR=/opt/mod-ui/html
export MOD_DEFAULT_PEDALBOARD=/opt/mod-ui/default.pedalboard
export MOD_LOG="${MOD_LOG:-1}"
echo "[mod-sim] mod-ui serving http://localhost:8888"
exec /opt/mod-ui-env/bin/python3 server.py
