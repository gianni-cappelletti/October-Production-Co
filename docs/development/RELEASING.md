### Creating a Release

A single git tag triggers CI to build and package all plugins. The CI workflow (`.github/workflows/release-juce.yml`) uses a matrix strategy to build both OctobIR and OctoBASS for every release.

#### Version Management

**All components use a single centralized version from the `VERSION` file at the project root.**

The version is automatically propagated to:
- CMake builds (all core libraries and JUCE plugins, via `OCTOBIR_VERSION` / `OCTOBASS_VERSION`)
- Windows installers (Inno Setup)
- macOS installers (PKG/DMG)

For VCV Rack, run the sync script to update `plugin.json`:
```bash
./scripts/sync-vcv-version.sh
```

#### JUCE Plugin Release

1. **Update the version number:**
    - Edit the `VERSION` file in the project root (e.g., change to `2.1.0`)
    - Sync VCV plugin version: `./scripts/sync-vcv-version.sh`
    - Commit the changes:
      ```bash
      git add VERSION plugins/octobir/vcv-rack/plugin.json
      git commit -m "Bump version to 2.1.0"
      ```

2. **Run quality checks:**
   ```bash
   make tidy                # Formatting, static analysis, license compliance
   make octobir-juce        # Test OctobIR release build in your DAW
   make octobass-juce       # Test OctoBASS release build in your DAW
   ```

3. **Create and push a git tag:**
   ```bash
   git tag -a v2.1.0 -m "v2.1.0: Brief description of changes"
   git push origin v2.1.0
   ```

   This single tag triggers builds for all plugins across all platforms.

4. **Automated build triggers:**
    - GitHub Actions automatically builds installers for all platforms:
        - macOS: `.pkg` installer in DMG with VST3 and AU
        - Windows: `.exe` installer with VST3
        - Linux: `.tar.gz` with install script for VST3
    - Creates a **draft** GitHub release with all artifacts attached

5. **Review and publish:**
    - Go to GitHub Releases page
    - Find the draft release
    - Add release notes describing changes
    - Click "Publish release" to make it public

**Note:** The release stays in draft mode until you manually publish it, allowing you to review artifacts before distribution.

#### VCV Rack Plugin Distribution (OctobIR Only)

VCV Rack plugins are distributed through the [VCV Library](https://library.vcvrack.com/), not GitHub releases. The Library build farm requires a standard Rack plugin (`plugin.json` + `Makefile`) at the **root** of the repository it builds, which this monorepo is not. OctobIR is therefore submitted through a dedicated packaging repository, [vcv-octobir](https://github.com/gianni-cappelletti/vcv-octobir), which pins this monorepo as a submodule (`opc`) and exposes a standard Rack plugin at its root. No plugin source is duplicated.

> The monorepo's `plugins/octobir/vcv-rack/plugin.json` is used only for local
> development and CMake builds. The manifest the Library actually builds from lives
> in the wrapper repo.

To release a VCV Rack update:

1. **Cut the monorepo release** as above (bump `VERSION`, run `./scripts/sync-vcv-version.sh`, commit, tag `vX.Y.Z`).
2. **Update the wrapper repo** ([vcv-octobir](https://github.com/gianni-cappelletti/vcv-octobir)):
   ```bash
   git -C opc fetch
   git -C opc checkout vX.Y.Z          # pin to the monorepo release tag
   git add opc
   # bump "version" in plugin.json to X.Y.Z to match
   git commit -m "Bump OctobIR to X.Y.Z"
   git tag -a vX.Y.Z -m "vX.Y.Z"
   git push --follow-tags
   ```
   CI cross-builds all four platforms (mac-x64, mac-arm64, win-x64, lin-x64) and attaches the `.vcvplugin` artifacts to a GitHub Release.
3. **Notify the VCV Library:**
    - First time: create an issue at [VCVRack/library](https://github.com/VCVRack/library) with the plugin slug and the **wrapper repo** as the source URL.
    - Updates: comment in the plugin's thread with the new version and the wrapper commit hash.
4. **Users download** via VCV Rack's built-in Plugin Manager.
