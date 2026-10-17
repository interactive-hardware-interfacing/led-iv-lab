<!-- Thanks for contributing. One logical change per PR, please. -->

## Summary

<!-- What does this change and why? -->
Closes #

## What this touches

- [ ] `index.html` (page)
- [ ] Arduino firmware
- [ ] CircuitPython firmware
- [ ] `PROTOCOL.md` — if checked, **all** of the above plus demo mode must change in this PR
- [ ] Docs / README / CHANGELOG
- [ ] Workflows

## How it was tested

- [ ] Demo mode walkthrough: connect → each LED → slider → sweep → table → CSV → disconnect
- [ ] Light **and** dark mode checked (page changes)
- [ ] Tested on a real ItsyBitsy M4 — firmware on the board: `ID LEDLAB …`
- [ ] Firmware: `HELP`, `ID`, `LED`, `SET`, `MEAS`, `STREAM`, `R`, `STATE` replies match PROTOCOL.md
- [ ] Firmware: unplugging USB while streaming turns the LED off

<!-- Paste the relevant serial log lines or a screenshot if useful. -->

## Checklist

- [ ] No new literal colours in the page — everything comes from the CSS variables
- [ ] No new dependencies (page stays a single self-contained file)
- [ ] CHANGELOG.md updated
- [ ] If this is a release PR: `FW_VERSION` bumped in both firmwares and the page footer

## Notes for reviewers

<!-- Anything tricky, or follow-ups deliberately left out. -->
