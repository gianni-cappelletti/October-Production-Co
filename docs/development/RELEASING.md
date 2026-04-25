### Creating a Release

Each plugin is released independently. The CI workflow (`.github/workflows/release-juce.yml`) determines which plugin to build based on the git tag prefix.

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

   Tags use per-plugin prefixes so CI knows which plugin to build:
   ```bash
   # OctobIR release
   git tag -a octobir-v2.1.0 -m "OctobIR v2.1.0: Brief description of changes"
   git push origin octobir-v2.1.0

   # OctoBASS release
   git tag -a octobass-v2.1.0 -m "OctoBASS v2.1.0: Brief description of changes"
   git push origin octobass-v2.1.0
   ```

   You can push both tags from the same commit if both plugins are being released together.

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

VCV Rack plugins are distributed through the [VCV Library](https://library.vcvrack.com/), not GitHub releases:

1. **Sync version** from the `VERSION` file: `./scripts/sync-vcv-version.sh`
2. **Test locally:**
   ```bash
   make octobir-vcv
   # Install and test in VCV Rack
   ```
3. **Commit and push:**
   ```bash
   git add plugins/octobir/vcv-rack/plugin.json
   git commit -m "Sync VCV plugin version"
   git push
   ```
4. **Submit to VCV Library:**
    - First time: Create issue at [VCV Library](https://github.com/VCVRack/library) with plugin info
    - Updates: Comment in your plugin's issue with version number and commit hash
    - VCV builds for all platforms automatically
5. **Users download** via VCV Rack's built-in Plugin Manager
