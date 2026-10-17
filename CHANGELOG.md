# Changelog

## v1.0.0 — first release

- Web instrument (`index.html`): Web Serial connection, LED select with lit
  indicators, drive-voltage slider, live voltage/current readouts, I–V chart
  with one series per LED, automatic sweep, table view, CSV export, serial log
  with manual command entry, demo mode with a simulated board, firmware
  instructions. Light and dark colour schemes.
- Arduino firmware for the ItsyBitsy M4 implementing the serial protocol in
  `PROTOCOL.md`, with a non-blocking CheckVoltage cycle and safe-off on USB
  disconnect.
- CircuitPython firmware speaking the same protocol, as a swap-in alternative.
- CI: Arduino build → `.uf2` attached to tagged releases; GitHub Pages deploy.
- Licensed GPL-2.0.

Known gaps, to be checked on hardware:
- DAC/ADC full-scale assumed to be exactly 3.3 V.
- `V_LED` is plotted as read at A1; confirm it is the LED forward voltage on
  the final board.
- Sense resistor defaults to a 100 Ω placeholder.
