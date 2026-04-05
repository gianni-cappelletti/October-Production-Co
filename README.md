#################################################################

     _____           __           __       ______   ____
    /\  __`\        /\ \__       /\ \     /\__  _\ /\  _`\
    \ \ \/\ \    ___\ \ ,_\   ___\ \ \____\/_/\ \/ \ \ \L\ \
     \ \ \ \ \  /'___\ \ \/  / __`\ \ '__`\  \ \ \  \ \ ,  /
      \ \ \_\ \/\ \__/\ \ \_/\ \L\ \ \ \L\ \  \_\ \__\ \ \\ \
       \ \_____\ \____\\ \__\ \____/\ \_,__/  /\_____\\ \_\ \_\
        \/_____/\/____/ \/__/\/___/  \/___/   \/_____/ \/_/\/ /

#################################################################

OctobIR is a free, open source impulse response loader with static and dynamics driven blending options. Load up two impulse responses (say, a guitar cabinet and a big reverb) and enable dynamic mode. Then the blend between the two IRs will change depending on the dynamics of your source input or a sidechained input. This is an effect I played around with in VCV Rack with a somewhat cumbersome and large patch, but wanted it in a simple all in one plugin that I could use in my DAW as well.

## Supported Platforms

- **VCV Rack** - Eurorack simulator plugin
- **VST3/AU** - DAW plugins via JUCE (macOS/Windows/Linux)

## Installation

### Option 1: Pre-built Installers (Recommended)

Download the latest release for your platform from [GitHub Releases](https://github.com/gianni-cappelletti/OctobIR/releases):

**macOS**
1. Download `OctobIR-macOS.dmg`
2. Open the DMG and run the installer
3. Select components to install (VST3, AU)
4. Complete installation

**Windows**
1. Download `OctobIR-Windows.exe`
2. Run the installer
3. Follow the installation wizard
4. VST3 will be installed to `C:\Program Files\Common Files\VST3\`

**Linux**
1. Download `OctobIR-Linux.tar.gz`
2. Extract: `tar -xzf OctobIR-Linux.tar.gz`
3. Run the installer: `cd OctobIR && ./install.sh`
4. VST3 will be installed to `~/.vst3/`

**VCV Rack**
- VCV Rack plugins are distributed through the [VCV Library](https://library.vcvrack.com/)
- Search for "OctobIR" in the VCV Rack plugin manager

### Option 2: Building from Source

#### Prerequisites

```bash
# Clone with submodules
git clone --recursive https://github.com/gianni-cappelletti/OctobIR.git
cd OctobIR

# Or if already cloned
git submodule update --init --recursive

# Initial setup (first time only - installs dependencies)
./scripts/setup-dev.sh
```

#### Build & Install

```bash
make juce    # Build and install JUCE plugins (VST3 + AU)
make vcv     # Build and install VCV Rack plugin
```

**Note**: If you previously installed via the packaged installer, remove the old plugins first:
```bash
sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3
sudo rm -rf ~/Library/Audio/Plug-Ins/Components/OctobIR.component
```

Installed locations:
- **macOS VST3**: `~/Library/Audio/Plug-Ins/VST3/OctobIR.vst3`
- **macOS AU**: `~/Library/Audio/Plug-Ins/Components/OctobIR.component`
- **Linux VST3**: `~/.vst3/OctobIR.vst3`
- **VCV Rack**: `~/Documents/Rack2/plugins/OctobIR` (or your Rack plugins directory)


## A note on the use of Claude Code

As a software engineer and a musician, I've spent a lot of time thinking about generative AI and its implications for both sides of my career. I've undergone several existential crises as it relates to my livelihood and the role of my creative work as an artist. In an effort to add some nuanced, principled thoughts to the cultural conversation beyond "AI is an unabated good" and "AI is a scourge on humanity", I've written up an essay here that I hope can provide some sound (no pun intended) thinking on the just and proper use of generative AI as it relates to art and craft.

[Art and Craft: On the use of generative AI](docs/ART_AND_CRAFT.txt)

## License

**OctobIR software is licensed under GPL-3.0 (GNU General Public License v3.0)**

This applies to all software components:
- Core library (`libs/octobir-core/`)
- JUCE plugin (VST3/AU)
- VCV Rack plugin

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
- **VCV Rack SDK** - VCV Rack plugin API - GPL-3.0+

See `docs/legal/LICENSING.md` and `docs/legal/THIRD_PARTY_NOTICES.txt` for detailed information.
