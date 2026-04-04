# OctobIR VCV Rack — Remaining Work

Phases 1–3 are complete and tested (21 tests passing, 1 skipped for mono IR).
Phase 4 (UI) and Phase 5 (SVG panel) remain.

---

## Phase 4: UI Overhaul

### Panel dimensions
**28HP** = 142.24 mm wide × 128.5 mm tall.

### Layout zones (Y in mm from top)

| Y range    | Content                                                        |
|------------|----------------------------------------------------------------|
| 0–5        | Top screws                                                     |
| 5–15       | IR A row: enable switch, trim knob, LCD display, Load/Clear/Prev/Next buttons |
| 15–25      | IR B row: same                                                 |
| 25–33      | Mode row: DYNAMIC toggle, SWAP button, SIDECHAIN toggle        |
| 33–48      | Meter panel: INPUT level bar + BLEND bar                       |
| 48–65      | Large knobs: BLEND, OUTPUT GAIN                                |
| 65–82      | Small knobs: THRESHOLD, RANGE, KNEE                            |
| 82–97      | Small knobs: ATTACK, RELEASE + DETECT toggle                   |
| 97–108     | Port row 1: In L, In R, Out L, Out R, SC In                   |
| 108–120    | Port row 2: THR CV, BLD CV, DYN EN                            |
| 120–128.5  | Bottom screws                                                  |

### Custom widgets to implement

**`IrFileDisplay`** (already exists — update):
- Widen to ~75 mm to show ~30 chars
- Add right-click menu item for Clear
- Keep click-to-load behavior

**`OpcMeterDisplay`** — new `TransparentWidget`:
- NanoVG segment drawing (19 segments)
- INPUT bar (left-fill from silence) and BLEND bar (center-balanced)
- Orange LCD background `#F08830`, dark segment fill `#1c1c30`
- Reads `module->currentInputLevelDb_` and `module->currentBlend_` atomics
- Null module check for browser preview

**`OpcRetroButton`** — toggle `SvgSwitch` for DYNAMIC and SIDECHAIN params:
- Dark `#1c1c30` background, orange `#F08830` border when lit
- Uses co-located `DynamicModeLight` / `SidechainLight`

**`OpcSwapButton`** — momentary `OpaqueWidget` (not a param):
- Calls `module->swapImpulseResponses()` on left-click
- Draws "SWAP" text via NanoVG in retro style

**`OpcIrLoadButton` / `OpcIrClearButton`** — small `OpaqueWidget` buttons per IR row:
- Load: opens file dialog via `osdialog_file`
- Clear: calls `irProcessor_.clearImpulseResponse1/2()` and clears path

**`OpcIrPrevButton` / `OpcIrNextButton`** — directory browsing buttons:
- Walk `std::filesystem::directory_iterator` for adjacent .wav/.aiff/.flac files
- Load the previous/next file in alphabetical order relative to the currently loaded IR

**`OpcTrimKnob`** — `RoundSmallBlackKnob` variant for ±12 dB IR trim gains

**`OpcIrEnableSwitch`** — `CKSS` (standard small Rack toggle) for IR A/B enable

### Widget constructor (`OpcVcvIrWidget`)
Full layout wiring: all params, inputs, outputs, lights, and custom widgets
positioned to the mm grid above. See plan file for exact coordinates.

### Context menu
Keep `appendContextMenu` with Dynamic Mode, Sidechain Enable, and Swap IR Order
(on-panel access is primary; context menu provides discoverability).

---

## Phase 5: SVG Panel Design

**File**: `plugins/vcv-rack/res/opc-vcv-ir-panel.svg`
Full replacement of the current 6HP panel.

### Spec
- Dimensions: `142.24 mm × 128.5 mm`, `viewBox="0 0 142.24 128.5"`
- Grid: 2.54 mm half-HP grid (use Inkscape)

### Color palette

| Element            | Color     |
|--------------------|-----------|
| Panel background   | `#2a2a3a` |
| LCD zone fills     | `#F08830` |
| LCD text / borders | `#1c1c30` |
| Knob/port labels   | `#F08830` |
| Button borders     | `#F08830` |

### SVG layers
1. **Background** — dark fill, mounting rail indicators
2. **LCD zones** — orange rectangles behind IR rows and meter panel (widgets draw on top)
3. **Labels** — all knob/port/section text in Inconsolata Condensed Bold
4. **Mounting holes** — screw cutouts at four corners

Widget positions in code must align with Inkscape label positions.
