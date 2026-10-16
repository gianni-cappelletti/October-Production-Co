# OctoBASS LV2 (MOD Audio)

Native LV2 wrapper around `octobass-core` for MOD Audio devices (Dwarf, Duo X) and LV2 hosts.

## Layout

| Path | Contents |
|------|----------|
| `src/` | Plugin (`OctoBassLv2`), port table, TTL generator |
| `bundle/` | Committed bundle data: `manifest.ttl.in`, `octobass.ttl`, `presets.ttl`, `modgui.ttl`, `modgui/` |
| `tools/` | `ModguiAssetRenderer`: renders modgui sprites, faceplate, screenshot, template and stylesheet with `OctoberLookAndFeel` |
| `tests/` | Behavioral tests driven through the LV2 C API by `FakeLv2Host` |
| `../../../installers/mod/octobass-lv2/` | mod-plugin-builder package recipe |

## Plugin interface

| Item | Value |
|------|-------|
| URI | `https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass` |
| Audio | 1 in, 1 out |
| Control ports | 74 inputs mirroring the JUCE parameters, `enabled` (bypass), `latency` output |
| File parameters | `#nam1Model`, `#nam2Model` (`mod:fileTypes "nammodel"`), `#cabinetIr` (`mod:fileTypes "cabsim"`, `.wav`) |
| Required features | `urid:map`, `work:schedule` |
| Extensions | `work:interface`, `state:interface` |
| Default NAM quality | 49% (slim submodel of slimmable models) |
| Version | Generated from `VERSION`: `lv2:minorVersion` = 2 x (MAJOR x 100 + MINOR), `lv2:microVersion` = 2 x PATCH (2.3.0 is 406.0) |

- Percent ports use 0-100; the core receives 0-1.
- File loads, clears and NAM quality changes run on the LV2 worker thread; `run()` does not allocate.
- Both Solo ports on: the most recently engaged solo wins. The modgui clears the other Solo port.
- Saved state stores file paths through `state:mapPath`.
- LV2 versions have no major field, and hosts treat an odd minor or micro version as a development build (mod-ui labels it Testing). The encoding keeps every release even and increasing. MINOR must stay below 100.
- The version is written into `manifest.ttl` and `modgui/icon.html` from their `.in` templates when the bundle is assembled.

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

## Benchmark

```bash
make bench-octobass-core
build/bench-octobass-core/libs/octobass-core/bench/octobass-core-bench \
    --nam1 <model.nam> --nam2 <model.nam> --ir <ir.wav> --quality 0 --block 128
```

Reports mean, 99th percentile and maximum block processing time as a percentage of the real-time budget.
