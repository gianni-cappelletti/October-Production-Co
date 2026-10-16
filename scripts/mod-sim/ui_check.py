import os
import sys
import urllib.parse

from playwright.sync_api import Page, expect, sync_playwright

MOD_UI_URL = os.environ.get("MOD_UI_URL", "http://127.0.0.1:8888")
OUTPUT_DIR = os.environ.get("MOD_SIM_OUTPUT", "/out")
PLUGIN_URI = "https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass"
INSTANCE = "/graph/octobass"
PEDAL = ".mod-pedal-octobass"

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
    for css_class in ["squash", "compression-mode", "low-level", "low-solo", "crossover", "gate",
                      "high-input-gain", "high-blend", "high-output-gain", "high-solo",
                      "nam1-on", "nam2-on", "bypass"]:
        check(f"widget {css_class} visible", widget(page, css_class).is_visible())
    for parameter in ["nam1Model", "nam2Model", "cabinetIr"]:
        check(f"file picker {parameter} rendered once", file_picker(page, parameter).count() == 1,
              f"count={file_picker(page, parameter).count()}")
    expected_readouts = {
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


def check_solo_exclusivity(page: Page):
    low = widget(page, "low-solo")
    high = widget(page, "high-solo")
    low.click()
    page.wait_for_timeout(400)
    check("low solo turns on", "on" in classes(low), " ".join(classes(low)))
    high.click()
    page.wait_for_timeout(600)
    check("high solo turns on", "on" in classes(high), " ".join(classes(high)))
    check("low solo cleared by modgui script", "off" in classes(low), " ".join(classes(low)))
    high.click()
    page.wait_for_timeout(400)


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


def check_pedal_size(page: Page):
    size = page.evaluate(
        """() => { const p = document.querySelector('.mod-pedal-octobass');
                   return [p.offsetWidth, p.offsetHeight]; }""")
    check("pedal is scaled up", size[0] >= 950 and size[1] >= 440, f"{size[0]}x{size[1]}")


def list_visible(page: Page, parameter):
    return file_picker(page, parameter).locator(".mod-enumerated-list").is_visible()


def check_file_list_dismissal(page: Page):
    file_picker(page, "nam1Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    check("file list opens", list_visible(page, "nam1Model"))
    box = page.locator(PEDAL).bounding_box()
    page.mouse.click(box["x"] + box["width"] + 200, box["y"] + box["height"] / 2)
    page.wait_for_timeout(300)
    check("click outside closes file list", not list_visible(page, "nam1Model"))

    file_picker(page, "nam1Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    file_picker(page, "nam2Model").locator(".mod-enumerated-selected").click()
    page.wait_for_timeout(300)
    check("opening another picker closes the first", not list_visible(page, "nam1Model"))
    check("second picker opens", list_visible(page, "nam2Model"))
    page.mouse.click(box["x"] + box["width"] + 200, box["y"] + box["height"] / 2)
    page.wait_for_timeout(300)
    check("click outside closes second picker", not list_visible(page, "nam2Model"))


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
        page.goto(MOD_UI_URL + "/")
        page.wait_for_load_state("networkidle")
        wait_until_ready(page)

        add_pedal(page)
        screenshot(page, "pedal-default.png")
        check_layout(page)
        check_pedal_size(page)
        check_sprite_frames(page)
        check_file_list_dismissal(page)
        check_solo_exclusivity(page)
        check_bypass(page)
        check_knob_drag(page)
        check_file_loading(page)
        screenshot(page, "pedal-loaded.png")
        page.screenshot(path=os.path.join(OUTPUT_DIR, "pedalboard.png"))

        browser.close()

    print(f"{len(failures)} failure(s)")
    for failure in failures:
        print(f"  {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
