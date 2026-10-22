# OctoBASS LV2 (MOD Audio)

Native LV2 wrapper around `octobass-core` for MOD Audio devices (Dwarf, Duo X) and LV2 hosts.

## Layout

| Path | Contents |
|------|----------|
| `src/` | Plugin (`OctoBassLv2`), port table, TTL generator |
| `bundle/` | Committed bundle data: `manifest.ttl.in`, `octobass.ttl`, `presets.ttl`, `modgui.ttl`, `modgui/`. `LICENSE` and `docs/legal/THIRD_PARTY_NOTICES.txt` are copied in when the bundle is assembled |
| `tools/` | `ModguiAssetRenderer`: renders modgui sprites, faceplate, screenshot, template and stylesheet with `OctoberLookAndFeel` |
| `tests/` | Behavioral tests driven through the LV2 C API by `FakeLv2Host` |
| `../../../installers/mod/octobass-lv2/` | mod-plugin-builder package recipe |

## Plugin interface

| Item | Value |
|------|-------|
| URI | `https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass` |
| Audio | 1 in, 1 out |
| Control ports | 20 inputs mirroring the JUCE parameters except the graphic EQ and NAM quality, `enabled` (bypass), `latency` output |
| File parameters | `#nam1Model`, `#nam2Model` (`mod:fileTypes "nammodel"`), `#cabinetIr` (`mod:fileTypes "cabsim"`, `.wav`) |
| Required features | `urid:map`, `work:schedule` |
| Extensions | `work:interface`, `state:interface` |
| NAM quality | Fixed at 0.49 (MOD Neural Amp Modeler's `DEFAULT_QUALITY_SCALE`): slimmable (A2) captures run their slim submodel. Not exposed as a port |
| Version | Generated from `VERSION`: `lv2:minorVersion` = 2 x (MAJOR x 100 + MINOR), `lv2:microVersion` = 2 x PATCH (2.4.0 is 408.0) |

- Percent ports use 0-100; the core receives 0-1.
- The graphic EQ is not exposed. Its low cut (20 Hz) and high cut (20 kHz) are always on, matching the JUCE defaults; the 16 nodes are off.
- Port names longer than mod-ui's 16-character display limit carry an `lv2:shortName`.
- The bundle includes `LICENSE` (GPL-3.0) and `THIRD_PARTY_NOTICES.txt`.
- File loads and clears run on the LV2 worker thread; `run()` does not allocate.
- Non-slimmable NAM captures (standard WaveNet, LSTM) always run at full size; full-size captures exceed the Dwarf CPU budget.
- The pedal has no Solo buttons; the Solo ports are in MOD's settings panel. Both Solo ports on: the most recently engaged solo wins, and the modgui clears the other Solo port.
- The footer has three footswitches with status LEDs: ON (plugin on/bypass), NAM 1 and NAM 2 (slot on/off). A lit LED means the plugin or slot is on. A NAM slot that is off dims its LCD backlight, as in the JUCE plugin.
- Each file list (pedal LCDs and settings panel) starts with a None row. It sends an empty path, which unloads the slot. An empty slot reads `-- none --`. `script.js` adds the row because mod-ui's file picker hides list entries that are not files.
- The file picker being used is raised above the other LCDs so its open list is not covered.
- Saved state stores file paths through `state:mapPath`.
- LV2 versions have no major field, and hosts treat an odd minor or micro version as a development build (mod-ui labels it Testing). The encoding keeps every release even and increasing. MINOR must stay below 100.
- The version is written into `manifest.ttl` from `manifest.ttl.in` when the bundle is assembled.

## Build

```bash
make octobass-lv2          # Release bundle, installed to ~/Library/Audio/Plug-Ins/LV2 or ~/.lv2
make test-octobass-lv2     # Behavioral tests (ASan)
make octobass-lv2-ttl      # Regenerate bundle/octobass.ttl and bundle/presets.ttl from the port table
make octobass-lv2-modgui   # Regenerate bundle/modgui/ (requires JUCE)
```

`Lv2BundleTests` fail when the committed TTL or preset differs from the port table.

## MOD device build

The Dwarf build uses MOD's toolchain (`moddwarf-new`: GCC 9.4, glibc 2.27, Cortex-A35).

```bash
docker build -t mpb:moddwarf-new --build-arg platform=moddwarf-new <mod-plugin-builder>/docker/
```

Inside the container, copy `installers/mod/octobass-lv2/octobass-lv2.mk` to `plugins/package/octobass-lv2/`, set `OCTOBASS_LV2_VERSION`, then:

```bash
./build moddwarf-new octobass-lv2            # Build
./validate moddwarf-new octobass-lv2         # TTL validation
./build moddwarf-new octobass-lv2-publish    # Install to a device connected over USB
```

## Desktop testing

Requires Docker (Colima on macOS). The simulator image runs JACK (dummy driver), mod-host and mod-ui at the commits pinned by MOD Desktop 0.0.13 (`scripts/mod-sim/Dockerfile`).

| Command | Result |
|---------|--------|
| `make octobass-mod-desktop` | Installs the macOS bundle to `~/Documents/MOD Desktop/lv2` and the bundled tones to `~/Documents/MOD Desktop/user-files` |
| `make mod-sim` | Builds OctoBASS from the working tree and serves mod-ui at http://localhost:18888 |
| `make mod-sim-check` | Headless Chromium checks against a running `mod-sim`: layout, readout formats, fonts, bypass, knob drag, file loading, list stacking and dismissal, None on the pedal and settings panel, and a cleared slot surviving pedalboard save and reload. Screenshots in `build/mod-sim/` |
| `make mod-sim-valgrind` | LV2 tests under valgrind (Linux arm64) |
| `make mod-sim-cpu` | Times OctoBASS and MOD's Neural Amp Modeler (mod-plugin-builder commit) in the same LV2 host |

Measured MOD Dwarf CPU (device tester, bundled A2 captures and cabinet IR):

| Configuration | CPU |
|---------------|-----|
| Plugin only | 10% |
| Plugin + IR | 22% |
| Plugin + 1 NAM + IR | 39-40.5% |
| Plugin + 2 NAM + IR | 59.5-60% |

CPU varies with the capture; one other lite capture reached 63% in a single slot.

`make mod-sim-cpu` estimate: `OctoBASS % / NAM nano % x Dwarf NAM nano %`. MOD forum reports for the Dwarf at 128 frames: NAM nano 36-42%, feather 66-76%. `make_timing_models.py` derives nano/feather/standard WaveNet models with random weights for timing only.

## Benchmark

```bash
make bench-octobass-core
build/bench-octobass-core/libs/octobass-core/bench/octobass-core-bench \
    --nam1 <model.nam> --nam2 <model.nam> --ir <ir.wav> --quality 0 --block 128
```

Reports mean, 99th percentile and maximum block processing time as a percentage of the real-time budget.
