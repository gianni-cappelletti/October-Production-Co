
## October Production Co.

Hey there! Welcome to the repo. There are two plugins in here at the moment: OctobIR and OctoBASS. 

- OctobIR is a dual impulse response loader with static and dynamic blending modes. Load up two different guitar cabs and blend between them statically, based on the guitar input volume, or even based on an external sidechain! 
- OctoBASS seeks to be an all-in-one bass plugin, akin to modern metal bass splitting plugins that have compression for the low end and distortion for the high end. Set the crossover frequency, compress the low end to sit well in a mix, and apply a NAM capture to the high band for an infinite amount of distortion options. It's intended to be a free open source alternative to modern bass plugins like Neural DSP's Parallax. 

## Supported Platforms

| Plugin | VST3 | AU | VCV Rack |
|--------|------|----|----------|
| OctobIR | macOS, Windows, Linux | macOS | macOS, Windows, Linux |
| OctoBASS | macOS, Windows, Linux | macOS | -- |

## Installation

### Option 1: Pre-built Installers (Recommended)

Download the latest release for your platform from [GitHub Releases](https://github.com/gianni-cappelletti/October-Production-Co/releases).

**macOS**
1. Download `OctobIR-macOS.dmg` and/or `OctoBASS-macOS.dmg`
2. Open the DMG and run the installer
3. Select components to install (VST3, AU)
4. Apple will try and block it as untrusted, as I am not going to pay them $99 a year for package signing rights on a FOSS project at this stage.
5. Go to System settings and allow the install anyway to complete setup

**Windows**
1. Download `OctobIR-Windows.exe` and/or `OctoBASS-Windows.exe`
2. Run the installer
3. Follow the installation wizard
4. VST3 will be installed to `C:\Program Files\Common Files\VST3\`

**Linux**
1. Download `OctobIR-Linux.tar.gz` and/or `OctoBASS-Linux.tar.gz`
2. Extract: `tar -xzf <plugin>-Linux.tar.gz`
3. Run the installer: `cd <plugin> && ./install.sh`
4. VST3 will be installed to `~/.vst3/`

**VCV Rack (OctobIR only)**
- VCV Rack plugins are distributed through the [VCV Library](https://library.vcvrack.com/)
- Search for "OctobIR" in the VCV Rack plugin manager

### Option 2: Building from Source

#### Prerequisites

```bash
# Clone with submodules
git clone --recursive https://github.com/gianni-cappelletti/October-Production-Co.git
cd October-Production-Co

# Or if already cloned
git submodule update --init --recursive

# Initial setup (first time only - installs dependencies)
./scripts/setup-dev.sh
```

#### Build & Install

```bash
make octobir-juce    # Build and install OctobIR JUCE plugins (VST3 + AU)
make octobir-vcv     # Build and install OctobIR VCV Rack plugin
make octobass-juce   # Build and install OctoBASS JUCE plugins (VST3 + AU)
```

**Note**: If you previously installed via the packaged installer, remove the old plugins first:
```bash
# OctobIR
sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3
sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctobIR.component

# OctoBASS
sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctoBASS.vst3
sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctoBASS.component
```

Installed locations:
- **macOS VST3**: `~/Library/Audio/Plug-Ins/VST3/<Plugin>.vst3`
- **macOS AU**: `~/Library/Audio/Plug-Ins/Components/<Plugin>.component`
- **Linux VST3**: `~/.vst3/<Plugin>.vst3`
- **VCV Rack** (OctobIR only): `~/Documents/Rack2/plugins/OctobIR` (or your Rack plugins directory)


## Reporting Issues

Found a bug, crash, or unexpected behavior? Please open an issue on [GitHub Issues](https://github.com/gianni-cappelletti/October-Production-Co/issues). When reporting, include your OS and version, plugin and host (DAW or VCV Rack) versions, and steps to reproduce. Logs or a short audio/video clip help a lot.

## A note on the use of Claude Code

As a software engineer and a musician, I've spent a lot of time thinking about generative AI and its implications for both sides of my career. I've undergone several existential crises as it relates to my livelihood and the role of my creative work as an artist. In an effort to add some nuanced, principled thoughts to the cultural conversation beyond "AI is an unabated good" and "AI is a scourge on humanity", I've written up an essay here that I hope can provide some sound (no pun intended) thinking on the just and proper use of generative AI as it relates to art and craft.

[Art and Craft: On the use of generative AI](docs/ART_AND_CRAFT.md)

## License

**All software in this repository is licensed under GPL-3.0 (GNU General Public License v3.0)**

This applies to all software components:
- Core libraries (`libs/octobir-core/`, `libs/octobass-core/`)
- Shared UI library (`libs/juce-ui-shared/`)
- OctobIR JUCE plugin (VST3/AU)
- OctobIR VCV Rack plugin
- OctoBASS JUCE plugin (VST3/AU)

The following non-software content is under separate licenses:
- **Brand assets** (October Production Co. name and logo) -- Proprietary trademark, not GPL. See `docs/legal/TRADEMARK.md`.
- **"Art and Craft" essay** (`docs/ART_AND_CRAFT.txt`) -- CC BY-ND 4.0. Share with attribution, no modifications.

Per-file license declarations follow the [REUSE specification](https://reuse.software). See `REUSE.toml` and `LICENSES/` for details.

See `LICENSE` for the full GPL text and `docs/legal/LICENSING.md` for detailed licensing information.

## Third-Party Components

- **JUCE** - Audio plugin framework (VST3/AU) - GPL-3.0
- **WDL** - DSP algorithms: convolution engine, FFT, resampling - zlib-style license
  - Source: https://github.com/justinfrankel/WDL (official Cockos repository)
- **pffft** - Fast Fourier Transform - BSD-style license
  - Source: https://github.com/marton78/pffft
- **dr_wav** - WAV file loading (header-only) - Public Domain
- **NeuralAmpModelerCore** - Neural amp modeling (OctoBASS) - MIT
  - Source: https://github.com/sdatkinson/NeuralAmpModelerCore
- **Eigen** - Linear algebra (used by NeuralAmpModelerCore) - MPL-2.0
- **nlohmann/json** - JSON parsing (used by NeuralAmpModelerCore) - MIT
- **VCV Rack SDK** - VCV Rack plugin API - GPL-3.0+

See `docs/legal/LICENSING.md` and `docs/legal/THIRD_PARTY_NOTICES.txt` for detailed information.
