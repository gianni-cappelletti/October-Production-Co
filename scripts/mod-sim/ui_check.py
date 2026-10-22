import os
import sys
import urllib.parse

from playwright.sync_api import Page, expect, sync_playwright

MOD_UI_URL = os.environ.get("MOD_UI_URL", "http://127.0.0.1:8888")
OUTPUT_DIR = os.environ.get("MOD_SIM_OUTPUT", "/out")
PLUGIN_URI = "https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass"
INSTANCE = "/graph/octobass"
PEDAL = ".mod-pedal-octobass"
SETTINGS = f'.mod-settings[mod-instance="{INSTANCE}"]'
FILE_PARAMETERS = ["nam1Model", "nam2Model", "cabinetIr"]
EMPTY_SLOT_TEXT = "-- none --"

failures = []


def check(name, condition, detail=""):
    print(("PASS " if condition else "FAIL ") + name + (f" ({detail})" if detail else ""))
    if not condition:
        failures.append(name)


def api(page: Page, path):
    response = page.request.get(MOD_UI_URL + path)
    check(f"GET {path.split('?')[0]}", response.ok, f"HTTP {response.status}")
    return response


def widget(page: Page, css_class):
    return page.locator(f"{PEDAL} .opc-{css_class}")


def file_picker(page: Page, parameter):
    return page.locator(f'{PEDAL} .opc-file[mod-parameter-uri$="#{parameter}"]')


def wait_until_ready(page: Page):
    expect(page.locator(".screen-loading")).to_be_hidden(timeout=30000)


def screenshot(page: Page, name):
    path = os.path.join(OUTPUT_DIR, name)
    page.locator(PEDAL).screenshot(path=path)
    print(f"Saved {path}")


def add_pedal(page: Page):
    if page.locator(PEDAL).count() > 0:
        api(page, f"/effect/remove{INSTANCE}")
        expect(page.locator(PEDAL)).to_have_count(0, timeout=10000)
    query = urllib.parse.urlencode({"uri": PLUGIN_URI, "x": 120, "y": 120})
    api(page, f"/effect/add{INSTANCE}?{query}")
    api(page, f"/effect/connect//graph/capture_1,{INSTANCE}/in")
    api(page, f"/effect/connect/{INSTANCE}/out,/graph/playback_1")
    expect(page.locator(PEDAL)).to_be_visible(timeout=20000)
    wait_until_ready(page)


def check_layout(page: Page):
    for css_class in ["squash", "compression-mode", "low-level", "crossover", "gate",
                      "high-input-gain", "high-blend", "high-output-gain",
                      "nam1-on", "nam2-on", "bypass"]:
        check(f"widget {css_class} visible", widget(page, css_class).is_visible())
    check("no NAM quality levers on the pedal", page.locator(f"{PEDAL} [opc-quality-symbol]").count() == 0)
    check("no version text on the pedal", page.locator(f"{PEDAL} .opc-version").count() == 0)
    for css_class in ["low-solo", "high-solo"]:
        check(f"no {css_class} button on the pedal", widget(page, css_class).count() == 0)
    for parameter in FILE_PARAMETERS:
        check(f"file picker {parameter} rendered once", file_picker(page, parameter).count() == 1,
              f"count={file_picker(page, parameter).count()}")
        shown = file_picker(page, parameter).locator(".mod-enumerated-selected").inner_text().strip()
        check(f"empty {parameter} slot reads {EMPTY_SLOT_TEXT}", shown == EMPTY_SLOT_TEXT, shown)
    expected_readouts = {
        "high-input-gain": "0.0 dB",
        "high-blend": "100%",
        "high-output-gain": "0.0 dB",
        "squash": "0%",
        "compression-mode": "Tight",
        "low-level": "0.0 dB",
        "crossover": "250 Hz",
        "gate": "Off",
    }
    for css_class, expected in expected_readouts.items():
        shown = widget(page, f"value-{css_class}").inner_text().strip()
        check(f"{css_class} readout matches JUCE format", shown == expected, shown)

    lcd_font = page.evaluate(
        """() => getComputedStyle(document.querySelector('.mod-pedal-octobass .opc-value')).fontFamily""")
    check("readouts use the LCD font", "OPC LCD" in lcd_font, lcd_font)
    file_font = page.evaluate(
        """() => getComputedStyle(document.querySelector('.mod-pedal-octobass .opc-file .mod-enumerated-selected')).fontFamily""")
    check("file names use the LCD font", "OPC LCD" in file_font, file_font)


def classes(locator):
    return (locator.get_attribute("class") or "").split()


def check_bypass(page: Page):
    bypass = widget(page, "bypass")
    bypass.click()
    page.wait_for_timeout(400)
    check("bypass engages", "on" in classes(bypass), " ".join(classes(bypass)))
    bypass.click()
    page.wait_for_timeout(400)
    check("bypass releases", "off" in classes(bypass), " ".join(classes(bypass)))


def check_sprite_frames(page: Page):
    mismatches = page.evaluate(
        """() => Array.from(document.querySelectorAll('.mod-pedal-octobass .opc-widget')).flatMap(e => {
             const style = getComputedStyle(e);
             const width = parseFloat(style.width);
             const strip = parseFloat(style.backgroundSize.split(' ')[0]);
             const frames = strip / width;
             return Number.isInteger(width) && Math.abs(frames - Math.round(frames)) < 1e-6
                 ? [] : [e.className + ' ' + style.width + ' ' + style.backgroundSize];
           })""")
    check("sprite frames are whole pixels", not mismatches, "; ".join(mismatches))


def check_right_zone(page: Page):
    squash = widget(page, "squash").bounding_box()
    for css_class in ["high-input-gain", "high-blend", "high-output-gain"]:
        box = widget(page, css_class).bounding_box()
        check(f"{css_class} knob is full size", abs(box["width"] - squash["width"]) < 1,
              f"{box['width']:.1f} vs {squash['width']:.1f}")

    tops = page.evaluate(
        """() => ['nam1Model', 'nam2Model', 'cabinetIr'].map(p =>
             document.querySelector(`.mod-pedal-octobass .opc-file[mod-parameter-uri$="#${p}"]`).offsetTop)""")
    check("LCDs are closer together", tops[1] - tops[0] <= 92 and tops[2] - tops[1] <= 92, str(tops))

    boxes = page.evaluate(
        """() => ['nam1Model', 'nam2Model', 'cabinetIr'].map(p => {
             const e = document.querySelector(`.mod-pedal-octobass .opc-file[mod-parameter-uri$="#${p}"]`);
             return [e.offsetLeft, e.offsetTop, e.offsetWidth, e.offsetHeight];
           })""")
    widths = [box[2] for box in boxes]
    check("LCD slots share one width", len(set(widths)) == 1, str(widths))
    knob_row = page.evaluate(
        """() => ['high-input-gain', 'high-output-gain'].map(c => {
             const e = document.querySelector('.mod-pedal-octobass .opc-' + c);
             return [e.offsetLeft, e.offsetLeft + e.offsetWidth];
           })""")
    left, _, width, _ = boxes[0]
    check("LCD slots span the high band knob row",
          left <= knob_row[0][0] and left + width >= knob_row[1][1],
          f"lcd {left}..{left + width} knobs {knob_row[0][0]}..{knob_row[1][1]}")


DIVIDER_Y_CSS = 305 * 1.3


def check_lower_readouts_align_with_ir(page: Page):
    centres = page.evaluate(
        """() => {
             const centre = e => e.offsetTop + e.offsetHeight / 2;
             const pedal = '.mod-pedal-octobass';
             return {
               ir: centre(document.querySelector(`${pedal} .opc-file[mod-parameter-uri$="#cabinetIr"]`)),
               output: centre(document.querySelector(`${pedal} .opc-value-low-level`)),
               gate: centre(document.querySelector(`${pedal} .opc-value-gate`)),
             };
           }""")
    for name in ["output", "gate"]:
        check(f"{name} readout lines up with the IR slot", abs(centres[name] - centres["ir"]) <= 2,
              f"{centres[name]} vs {centres['ir']}")


def check_power_row(page: Page):
    positions = page.evaluate(
        """() => ['bypass', 'nam1-on', 'nam2-on'].map(c => {
             const e = document.querySelector('.mod-pedal-octobass .opc-' + c);
             return [e.offsetLeft, e.offsetTop, e.offsetHeight];
           })""")
    for name, (left, top, height) in zip(["bypass", "nam1-on", "nam2-on"], positions):
        check(f"{name} sits below the divider", top >= DIVIDER_Y_CSS, f"top={top}")
    lefts = [left for left, _, _ in positions]
    check("power row order is bypass, NAM 1, NAM 2", lefts == sorted(lefts), str(lefts))

    lcd_parameter = {"nam1": "nam1Model", "nam2": "nam2Model"}
    other = {"nam1": "nam2", "nam2": "nam1"}

    def lcd_dimmed(slot):
        return "opc-dim" in classes(file_picker(page, lcd_parameter[slot]))

    for slot in ["nam1", "nam2"]:
        check(f"{slot} LCD is lit while the slot is on", not lcd_dimmed(slot))
        lever = widget(page, f"{slot}-on")
        lever.click()
        page.wait_for_timeout(400)
        check(f"{slot} power lever switches off", "off" in classes(lever), " ".join(classes(lever)))
        check(f"{slot} LCD dims when the slot is off", lcd_dimmed(slot))
        check(f"{other[slot]} LCD stays lit", not lcd_dimmed(other[slot]))
        check("IR LCD never dims", "opc-dim" not in classes(file_picker(page, "cabinetIr")))
        lever.click()
        page.wait_for_timeout(400)
        check(f"{slot} power lever switches on", "on" in classes(lever), " ".join(classes(lever)))
        check(f"{slot} LCD lights again when the slot is on", not lcd_dimmed(slot))


def check_pedal_size(page: Page):
    size = page.evaluate(
        """() => { const p = document.querySelector('.mod-pedal-octobass');
                   return [p.offsetWidth, p.offsetHeight]; }""")
    check("pedal is scaled up", size[0] >= 950 and size[1] >= 500, f"{size[0]}x{size[1]}")
    check("pedal is no taller than its content needs", size[1] <= 580, f"{size[0]}x{size[1]}")


def background_luminance(page: Page, points):
    return page.evaluate(
        """async points => {
             const pedal = document.querySelector('.mod-pedal-octobass');
             const style = getComputedStyle(pedal);
             const url = style.backgroundImage.slice(5, -2);
             const image = new Image();
             await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = reject; image.src = url; });
             const canvas = document.createElement('canvas');
             canvas.width = image.naturalWidth;
             canvas.height = image.naturalHeight;
             const context = canvas.getContext('2d');
             context.drawImage(image, 0, 0);
             const scale = image.naturalWidth / pedal.offsetWidth;
             return points.map(([x, y]) => {
               const [r, g, b] = context.getImageData(Math.round(x * scale), Math.round(y * scale), 1, 1).data;
               return Math.round(0.2126 * r + 0.7152 * g + 0.0722 * b);
             });
           }""", points)


def name_strip(page: Page):
    return page.evaluate(
        """async () => {
             const pedal = document.querySelector('.mod-pedal-octobass');
             const url = getComputedStyle(pedal).backgroundImage.slice(5, -2);
             const image = new Image();
             await new Promise((resolve, reject) => { image.onload = resolve; image.onerror = reject; image.src = url; });
             const canvas = document.createElement('canvas');
             canvas.width = image.naturalWidth;
             canvas.height = image.naturalHeight;
             const context = canvas.getContext('2d');
             context.drawImage(image, 0, 0);
             const scale = image.naturalWidth / pedal.offsetWidth;
             const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
             const at = (x, y) => { const i = (y * canvas.width + x) * 4; return [pixels[i], pixels[i + 1], pixels[i + 2]]; };
             const luminance = ([r, g, b]) => 0.2126 * r + 0.7152 * g + 0.0722 * b;
             const probeX = Math.round(canvas.width * 0.1);
             let top = Math.round(canvas.height * 0.85);
             while (top < canvas.height && luminance(at(probeX, top)) > 120) top++;
             const inside = Math.min(canvas.height - 1, top + Math.round(4 * scale));
             const stripLuminance = luminance(at(probeX, inside));
             let bottom = inside;
             while (bottom < canvas.height && Math.abs(luminance(at(probeX, bottom)) - stripLuminance) <= 15) bottom++;
             const middle = Math.round((top + bottom) / 2);
             let left = canvas.width, right = -1;
             for (let y = top; y < bottom; y++)
               for (let x = 0; x < canvas.width; x++) {
                 const [r, g, b] = at(x, y);
                 if (r > 200 && g > 100 && g < 170 && b < 100) { left = Math.min(left, x); right = Math.max(right, x); }
               }
             return {
               top: top / scale,
               bottom: bottom / scale,
               colour: at(probeX, middle),
               rimLeft: at(Math.round(1.5 * scale), middle),
               below: at(probeX, canvas.height - Math.round(6 * scale)),
               text: right < 0 ? null : [left / scale, right / scale],
             };
           }""")


def relative_luminance(rgb):
    channels = [value / 255 for value in rgb]
    linear = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(a, b):
    high, low = sorted([relative_luminance(a), relative_luminance(b)], reverse=True)
    return (high + 0.05) / (low + 0.05)


def check_name_strip(page: Page):
    width, height = page.evaluate(
        """() => { const p = document.querySelector('.mod-pedal-octobass');
                   return [p.offsetWidth, p.offsetHeight]; }""")
    strip = name_strip(page)
    check("name strip runs along the bottom of the faceplate", strip["bottom"] - strip["top"] >= 20,
          f"strip {strip['top']:.1f}..{strip['bottom']:.1f}px")
    strip_luminance = relative_luminance(strip["colour"])
    check("name strip face is black", strip_luminance <= 0.012,
          f"{strip['colour']} luminance {strip_luminance:.3f}")
    check("bevel stays visible along the strip's side",
          abs(relative_luminance(strip["rimLeft"]) - strip_luminance) >= 0.01,
          f"rim {strip['rimLeft']} strip {strip['colour']}")
    check("enclosure side shows below the strip",
          relative_luminance(strip["below"]) >= strip_luminance + 0.03,
          f"below {strip['below']} strip {strip['colour']}")
    text = strip["text"]
    check("strip carries a name in the LCD orange", text is not None, str(text))
    if text is not None:
        centre = (text[0] + text[1]) / 2
        check("name is centered on the strip", abs(centre - width / 2) <= 3,
              f"centre {centre:.1f} pedal centre {width / 2}")
        check("name is wide enough to read", text[1] - text[0] >= 80, f"{text[1] - text[0]:.1f}px")
    ratio = contrast((0xF0, 0x88, 0x30), strip["colour"])
    check("name meets WCAG AA contrast on the strip", ratio >= 4.5, f"{ratio:.2f}:1")

    lowest = page.evaluate(
        """() => Math.max(...['bypass', 'nam1-on', 'nam2-on'].map(c => {
             const e = document.querySelector('.mod-pedal-octobass .opc-' + c);
             return e.offsetTop + e.offsetHeight;
           }))""")
    check("footswitches clear the name strip", strip["top"] - lowest >= 4,
          f"gap {strip['top'] - lowest:.1f}px")


def check_enclosure(page: Page):
    style = page.evaluate(
        """() => { const s = getComputedStyle(document.querySelector('.mod-pedal-octobass'));
                   return [s.borderTopLeftRadius, s.boxShadow]; }""")
    check("enclosure has rounded corners", float(style[0].rstrip("px") or 0) >= 12, style[0])
    check("pedal casts a shadow on the floor", style[1] not in ("", "none"), style[1])

    width, height = page.evaluate(
        """() => { const p = document.querySelector('.mod-pedal-octobass');
                   return [p.offsetWidth, p.offsetHeight]; }""")
    lcd = page.evaluate(
        """() => { const e = document.querySelector('.mod-pedal-octobass .opc-file[mod-parameter-uri$="#nam1Model"]');
                   return [e.offsetLeft, e.offsetTop, e.offsetWidth, e.offsetHeight]; }""")
    face, bottom_edge, top_edge, left_of_lcd, above_lcd = background_luminance(page, [
        [width / 2, height * 0.45],
        [width / 2, height - 4],
        [width / 2, 1.5],
        [lcd[0] - 1, lcd[1] + lcd[3] / 2],
        [lcd[0] + lcd[2] / 2, lcd[1] - 1],
    ])
    check("enclosure shows its depth along the bottom edge", bottom_edge < face - 40,
          f"edge {bottom_edge} face {face}")
    check("enclosure rim catches the light along the top edge", top_edge > face,
          f"edge {top_edge} face {face}")
    check("file LCDs sit in a recess darker than the panel",
          left_of_lcd < face - 40 and above_lcd < face - 40,
          f"left {left_of_lcd} above {above_lcd} face {face}")
    check("LCD recess is a soft shade, not a black outline",
          left_of_lcd > 90 and above_lcd > 90, f"left {left_of_lcd} above {above_lcd}")


def list_visible(page: Page, parameter):
    return file_picker(page, parameter).locator(".mod-enumerated-list").is_visible()


def click_outside_pedal(page: Page):
    box = page.locator(PEDAL).bounding_box()
    page.mouse.click(box["x"] + box["width"] + 200, box["y"] + box["height"] / 2)
    page.wait_for_timeout(300)


def check_file_list_dismissal(page: Page):
    file_picker(page, "nam1Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    check("file list opens", list_visible(page, "nam1Model"))
    box = page.locator(PEDAL).bounding_box()
    page.mouse.click(box["x"] + box["width"] + 200, box["y"] + box["height"] / 2)
    page.wait_for_timeout(300)
    check("click outside closes file list", not list_visible(page, "nam1Model"))

    file_picker(page, "nam2Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    file_picker(page, "nam1Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    check("opening another picker closes the first", not list_visible(page, "nam2Model"))
    check("second picker opens", list_visible(page, "nam1Model"))
    page.mouse.click(box["x"] + box["width"] + 200, box["y"] + box["height"] / 2)
    page.wait_for_timeout(300)
    check("click outside closes second picker", not list_visible(page, "nam1Model"))

    for parameter in ["nam1Model", "nam2Model"]:
        file_picker(page, parameter).locator(".mod-enumerated-selected").click()
        page.wait_for_timeout(300)
        covered = page.evaluate(
            f"""() => {{
                 const picker = document.querySelector('.mod-pedal-octobass .opc-file[mod-parameter-uri$="#{parameter}"]');
                 const list = picker.querySelector('.mod-enumerated-list').getBoundingClientRect();
                 const lcds = Array.from(document.querySelectorAll('.mod-pedal-octobass .opc-file'))
                     .filter(e => e !== picker).map(e => e.getBoundingClientRect());
                 return lcds.filter(r => r.top < list.bottom && r.bottom > list.top).flatMap(r => {{
                     const hit = document.elementFromPoint(r.left + r.width / 2, Math.max(r.top, list.top) + 4);
                     return picker.contains(hit) ? [] : [hit.className];
                 }});
               }}""")
        check(f"open {parameter} list is drawn above the LCDs below it", not covered, str(covered))
        click_outside_pedal(page)


def check_file_loading(page: Page):
    for parameter, expected_suffix in [("nam1Model", ".nam"), ("cabinetIr", ".wav")]:
        picker = file_picker(page, parameter)
        picker.locator(".mod-enumerated-selected").click()
        options = picker.locator('[mod-role="enumeration-option"]')
        expect(options.first).to_be_visible(timeout=5000)
        check(f"{parameter} lists files", options.count() > 0, f"count={options.count()}")
        name = options.first.inner_text().strip()
        options.first.click()
        wait_until_ready(page)
        page.wait_for_timeout(500)
        shown = picker.locator(".mod-enumerated-selected").inner_text().strip()
        check(f"{parameter} shows loaded file", name in shown and shown.endswith(expected_suffix),
              shown)


class PluginFileFeedback:
    def __init__(self, page: Page):
        self.reports = []
        page.on("websocket", lambda socket: socket.on("framereceived", self._receive))

    def _receive(self, frame):
        if not isinstance(frame, str) or not frame.startswith(f"patch_set {INSTANCE} "):
            return
        parts = frame.split(" ", 5)
        if len(parts) >= 5 and parts[4] == "p":
            self.reports.append((parts[3].rsplit("#", 1)[-1], parts[5] if len(parts) > 5 else ""))

    def clear(self):
        self.reports.clear()

    def last(self, parameter):
        paths = [path for name, path in self.reports if name == parameter]
        return paths[-1] if paths else None


def none_row(container):
    return container.locator(".mod-enumerated-list > .opc-file-none")


def settings_file_list(page: Page, parameter):
    return page.locator(SETTINGS).locator(f'.mod-file-list:has([mod-parameter-uri$="#{parameter}"])')


def shown_file(page: Page, parameter):
    return file_picker(page, parameter).locator(".mod-enumerated-selected").inner_text().strip()


def load_first_file(page: Page, parameter):
    picker = file_picker(page, parameter)
    picker.locator(".mod-enumerated-selected").click()
    option = picker.locator('[mod-role="enumeration-option"]:not(.mod-hidden)').first
    expect(option).to_be_visible(timeout=5000)
    option.click()
    wait_until_ready(page)
    page.wait_for_timeout(500)


def check_file_clearing(page: Page, feedback: PluginFileFeedback):
    for parameter in FILE_PARAMETERS:
        picker = file_picker(page, parameter)
        if shown_file(page, parameter) == EMPTY_SLOT_TEXT:
            load_first_file(page, parameter)
        check(f"{parameter} has a file before clearing", shown_file(page, parameter) != EMPTY_SLOT_TEXT,
              shown_file(page, parameter))

        picker.locator(".mod-enumerated-selected").click()
        page.wait_for_timeout(300)
        row = none_row(picker)
        check(f"{parameter} list offers None", row.count() == 1 and row.is_visible(),
              f"count={row.count()}")
        if row.count() != 1:
            click_outside_pedal(page)
            continue
        first_row = picker.locator(".mod-enumerated-list > div:not(.mod-hidden)").first
        check(f"{parameter} None is the first row", "opc-file-none" in classes(first_row),
              " ".join(classes(first_row)))
        check(f"{parameter} None row reads None", row.inner_text().strip() == "None",
              row.inner_text().strip())

        feedback.clear()
        row.click()
        page.wait_for_timeout(800)
        check(f"{parameter} LCD reads {EMPTY_SLOT_TEXT} after None",
              shown_file(page, parameter) == EMPTY_SLOT_TEXT, shown_file(page, parameter))
        check(f"{parameter} file list closes after None", not list_visible(page, parameter))
        check(f"OctoBASS reports {parameter} empty after None", feedback.last(parameter) == "",
              repr(feedback.last(parameter)))


def check_cleared_slot_survives_pedalboard_reload(page: Page, feedback: PluginFileFeedback):
    load_first_file(page, "nam1Model")
    load_first_file(page, "nam2Model")
    file_picker(page, "nam2Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    row = none_row(file_picker(page, "nam2Model"))
    if row.count() == 1:
        row.click()
        page.wait_for_timeout(800)
    else:
        click_outside_pedal(page)

    response = page.request.post(MOD_UI_URL + "/pedalboard/save",
                                 form={"title": "OctoBASS None check", "asNew": "1"})
    saved = response.ok and response.json().get("ok", False)
    check("pedalboard saves", saved, response.text())
    if not saved:
        return
    bundlepath = response.json()["bundlepath"]

    feedback.clear()
    response = page.request.post(MOD_UI_URL + "/pedalboard/load_bundle/",
                                 form={"bundlepath": bundlepath, "isDefault": "0"})
    check("pedalboard reloads", response.ok, response.text())
    page.reload()
    page.wait_for_load_state("networkidle")
    wait_until_ready(page)
    expect(page.locator(PEDAL)).to_be_visible(timeout=20000)
    page.wait_for_timeout(1500)

    check("loaded slot keeps its file after reload", shown_file(page, "nam1Model").endswith(".nam"),
          shown_file(page, "nam1Model"))
    check(f"cleared slot reads {EMPTY_SLOT_TEXT} after reload",
          shown_file(page, "nam2Model") == EMPTY_SLOT_TEXT, shown_file(page, "nam2Model"))
    nam2_reports = [path for name, path in feedback.reports if name == "nam2Model"]
    check("OctoBASS never reloads the cleared slot", all(path == "" for path in nam2_reports),
          str(nam2_reports))
    page.request.get(MOD_UI_URL + "/pedalboard/remove?" +
                     urllib.parse.urlencode({"bundlepath": bundlepath}))


def check_settings_panel_none(page: Page, feedback: PluginFileFeedback):
    for parameter in FILE_PARAMETERS:
        row = none_row(settings_file_list(page, parameter))
        check(f"settings panel {parameter} list offers None", row.count() == 1, f"count={row.count()}")

    if shown_file(page, "nam1Model") == EMPTY_SLOT_TEXT:
        load_first_file(page, "nam1Model")
    page.evaluate(f"""() => $('{SETTINGS}').window('open')""")
    page.wait_for_timeout(300)
    settings_row = none_row(settings_file_list(page, "nam1Model"))
    feedback.clear()
    if settings_row.count() == 1:
        settings_row.click()
        page.wait_for_timeout(800)
    page.evaluate(f"""() => $('{SETTINGS}').window('close')""")
    check(f"None in the settings panel shows {EMPTY_SLOT_TEXT} on the pedal",
          shown_file(page, "nam1Model") == EMPTY_SLOT_TEXT, shown_file(page, "nam1Model"))
    check("OctoBASS reports nam1Model empty after None in the settings panel",
          feedback.last("nam1Model") == "", repr(feedback.last("nam1Model")))


def check_knob_drag(page: Page):
    knob = widget(page, "squash")
    box = knob.bounding_box()
    page.mouse.move(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
    page.mouse.down()
    page.mouse.move(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2 - 60, steps=10)
    page.mouse.up()
    page.wait_for_timeout(500)
    readout = widget(page, "value-squash").inner_text()
    check("squash knob drag changes readout", readout.strip().endswith("%") and readout.strip() != "0%",
          readout)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch()
        page = browser.new_page(viewport={"width": 1600, "height": 1000}, device_scale_factor=2)
        page.on("pageerror", lambda error: failures.append(f"page error: {error}"))
        feedback = PluginFileFeedback(page)
        page.goto(MOD_UI_URL + "/")
        page.wait_for_load_state("networkidle")
        wait_until_ready(page)

        add_pedal(page)
        screenshot(page, "pedal-default.png")
        check_layout(page)
        check_pedal_size(page)
        check_enclosure(page)
        check_name_strip(page)
        check_right_zone(page)
        check_power_row(page)
        check_lower_readouts_align_with_ir(page)
        check_sprite_frames(page)
        check_file_list_dismissal(page)
        check_bypass(page)
        check_knob_drag(page)
        check_file_loading(page)
        screenshot(page, "pedal-loaded.png")
        check_file_clearing(page, feedback)
        check_cleared_slot_survives_pedalboard_reload(page, feedback)
        check_settings_panel_none(page, feedback)
        page.screenshot(path=os.path.join(OUTPUT_DIR, "pedalboard.png"))

        browser.close()

    print(f"{len(failures)} failure(s)")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
