# OctobIR Licensing Information

## Project License

**OctobIR is licensed under GPL-3.0 (GNU General Public License v3.0)**

This means:
- **Free and open source** - Anyone can use, modify, and distribute
- **Copyleft** - Derivative works must also be GPL-3.0
- **Commercial use allowed** - You can sell the software
- **Patent protection** - Contributors grant patent rights
- **No proprietary derivatives** - All modifications must remain open source

## Why GPL-3.0?

This license was chosen because:

1. **JUCE Framework Requirement**: JUCE is dual-licensed (GPL-3.0 or commercial). Using GPL-3.0 allows us to use JUCE for free in an open-source project.

2. **VCV Rack Requirement**: VCV Rack plugins must be licensed under GPL-3.0 or later.

3. **Community Alignment**: Audio plugin developers expect copyleft licensing for open-source projects to prevent proprietary forks.

4. **Compatibility**: GPL-3.0 is compatible with all our dependencies (WDL's zlib-style license and dr_wav's public domain/MIT license are permissive and GPL-compatible).

## Dependency Licenses

All third-party dependencies are GPL-compatible:

| Dependency | License | Usage | Compatibility |
|------------|---------|-------|---------------|
| **JUCE** | GPL-3.0 (or commercial) | Plugin framework (VST3/AU/Standalone) | GPL-3.0 |
| **WDL** | Zlib-style (permissive) | DSP algorithms (convolution, FFT, resampling) | GPL-compatible |
| **dr_wav** | Public Domain / MIT | WAV file loading | GPL-compatible |
| **VCV Rack SDK** | GPL-3.0+ | VCV Rack plugin API | GPL-3.0 |
| **Courier Prime** | SIL Open Font License 1.1 | UI typeface (bundled) | GPL-compatible |
| **Press Start 2P** | SIL Open Font License 1.1 | LCD display typeface (bundled) | GPL-compatible |

### WDL (Winamp Developmental Library)

**We use WDL from the official Cockos repository:**

- **WDL** is a collection of DSP utilities from Cockos Incorporated
- Repository: https://github.com/justinfrankel/WDL (official GitHub mirror)
- We only use these 3 WDL components for DSP processing:
  - `convoengine.cpp` - FFT-based convolution
  - `fft.c` - Fast Fourier Transform
  - `resample.cpp` - Sample rate conversion
- Licensed under a permissive zlib-style license (GPL-compatible)

**Our technology stack:**
```
┌─────────────────────────────────────┐
│   JUCE Plugin Framework (GPL-3.0)   │  ← Plugin wrapper (VST3/AU/Standalone)
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   octobir-core (our code)          │  ← Core DSP logic (GPL-3.0)
└────────────┬────────────────────────┘
             │
      ┌──────┴──────┐
      ▼             ▼
┌──────────┐  ┌──────────────┐
│   WDL    │  │   dr_wav     │         ← Low-level libraries
│ (zlib)   │  │ (public dom) │
└──────────┘  └──────────────┘
```

## License Compatibility Summary

- All dependencies are GPL-3.0 compatible
- Project can be distributed as GPL-3.0
- No licensing conflicts
- Commercial use is allowed under GPL-3.0 terms

## For Contributors

By contributing to OctobIR, you agree that your contributions will be licensed under GPL-3.0. This ensures:
- Your code remains open source
- You retain copyright to your contributions
- The project can be distributed freely
- Derivative works must also be open source

## For Users

Under GPL-3.0, you may:
- Use the software for any purpose (including commercial)
- Study and modify the source code
- Distribute the software
- Distribute modified versions

You must:
- Provide source code when distributing
- License derivatives under GPL-3.0
- Include copyright and license notices
- State significant changes made

## Questions?

- JUCE GPL-3.0 info: https://juce.com/juce-7-licence/
- GPL-3.0 full text: https://www.gnu.org/licenses/gpl-3.0.html
- GPL FAQ: https://www.gnu.org/licenses/gpl-faq.html