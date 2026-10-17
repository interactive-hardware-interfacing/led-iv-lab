# Contributing to LED I–V Lab

Thanks for helping. This is a small project with three moving parts — a web
page, two interchangeable firmwares, and the serial protocol that joins them —
and most contributions touch only one of the three.

Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) first.

## Ways to contribute

- **Report a bug** — use the *Bug report* template. Say whether you were on the
  live page or a local copy, which browser, and what the serial log showed.
- **Report a measurement that looks wrong** — use the *Measurement problem*
  template. This is the one for "the current reads 0", "the DAC tops out at
  3.1 V", "the green curve is in the wrong place". Include the CSV export if
  you have it; it's the fastest way to see what the board actually said.
- **Suggest an improvement** — use the *Feature request* template. Lab
  instructors: things that would make the page easier to use in a 2-hour
  section are especially welcome.
- **Improve the docs** — README, PROTOCOL.md, the on-page firmware steps.
  These need no hardware.
- **Submit code** — see below.

## The one rule: the protocol is the contract

`PROTOCOL.md` defines how the page and the firmware talk. Both firmwares must
answer every command identically, and the page must never depend on which one
is flashed.

- Changing the protocol means changing **PROTOCOL.md, both firmwares, the
  page, and the demo mode** in the same PR. A PR that changes one without the
  others will be sent back.
- Adding a command is fine; changing the meaning of an existing one needs a
  version bump and a changelog entry explaining why.
- `ID` must keep answering `ID LEDLAB …`. The page refuses to talk to anything
  else, on purpose.

## Development setup

There is no build step. Clone the repo and open `index.html` in Chrome or Edge.
Web Serial works from a `file://` URL, so you can test against a real board
without a server.

**Page.** Everything is in `index.html` — styles, markup, and script. Keep it
that way: a student should be able to view source and see the whole
instrument. No frameworks, no bundler, no CDN dependencies beyond the two
Google Fonts (the page must still work if those fail to load).

**Arduino firmware.** `firmware/arduino/led_iv_lab/led_iv_lab.ino`. Arduino
IDE with the Adafruit SAMD board package, target *Adafruit ItsyBitsy M4*, or:

```sh
arduino-cli core install adafruit:samd --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli compile --fqbn adafruit:samd:adafruit_itsybitsy_m4 --output-dir build firmware/arduino/led_iv_lab
```

**CircuitPython firmware.** `firmware/circuitpython/code.py`. Copy to the
`CIRCUITPY` drive of a board running CircuitPython 9.x.

## Testing

CI can compile the Arduino sketch and check the YAML, but it cannot plug in a
board. Everything that matters is tested by a human:

| Change touches… | Test before opening the PR |
|---|---|
| `index.html` | Run through the demo mode: connect, select each LED, move the slider, run a sweep, open the table, download a CSV, disconnect. Check the serial log shows sensible traffic. If you have a board, repeat against it. Check light **and** dark mode. |
| Either firmware | Flash it, type `HELP`, `ID`, `LED G`, `SET 1.5`, `MEAS`, `STREAM 1`, `STREAM 0`, `LED OFF` in the page's serial log (or any terminal) and confirm the replies match PROTOCOL.md. Unplug USB while streaming and confirm the LED goes off. |
| `PROTOCOL.md` | Both firmwares and the page updated to match; demo mode too. |
| Workflows | `python -c "import yaml; yaml.safe_load(open('.github/workflows/<file>.yml'))"` at minimum; a `workflow_dispatch` run on your fork is better. |

Say what you tested in the PR — the template asks. "Tested on hardware" means
a real ItsyBitsy M4 on the lab board; say which firmware was on it.

## Style

- **Page:** two-space indent, no semicolon omission, no new global names
  outside the IIFE. Colours come from the CSS variables at the top of the file;
  don't introduce literal hex values in markup or script. The four LED colours
  are reserved for data — nothing else on the page may use them.
- **Arduino:** keep it readable top to bottom. No dynamic allocation, no
  blocking `delay()` in the main loop (the measurement cycle is a state machine
  for a reason).
- **CircuitPython:** standard library only — no `lib/` dependencies, so the
  file is a single copy onto the drive.
- **Copy and docs:** sentence case, plain words, written for a student who has
  never seen a serial port. Explain what to do, not how the system is built.

## Commit and PR conventions

- One logical change per PR.
- Reference the issue it closes (`Closes #12`).
- Bump `FW_VERSION` in **both** firmwares and the version in the page footer
  together, only in release PRs.
- Add a line to `CHANGELOG.md` under the next version.

## Releases

Maintainers tag `vX.Y.Z` on `main`. The *firmware* workflow builds the `.uf2`
and attaches it to the release; the page's download link always points at the
latest release. See the *Releasing* section of the README.

## License

By contributing, you agree that your contributions are licensed under the
project's **GPL-2.0** license.
