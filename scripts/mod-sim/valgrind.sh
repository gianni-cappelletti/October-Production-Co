#!/bin/bash
set -euo pipefail

cmake -S /src -B /work/valgrind-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_OCTOBIR=OFF -DBUILD_OCTOBIR_JUCE=OFF -DBUILD_OCTOBIR_VCV=OFF -DBUILD_OCTOBASS_JUCE=OFF \
    -DBUILD_OCTOBASS_LV2=ON -DBUILD_OCTOBASS_LV2_TESTS=ON -DOCTOBASS_NAM_A2_FAST=ON >/dev/null
cmake --build /work/valgrind-build --target octobass-lv2-tests >/dev/null
valgrind --error-exitcode=99 --leak-check=full --errors-for-leak-kinds=definite,indirect \
    /work/valgrind-build/plugins/octobass/lv2/tests/octobass-lv2-tests
