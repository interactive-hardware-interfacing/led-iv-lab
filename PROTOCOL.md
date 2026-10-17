# LED I–V Lab — serial protocol

The web page, the firmware, and any script a student writes all speak this
protocol. It is the contract between them: either firmware (Arduino or
CircuitPython) can be flashed and the page does not care which.

- Transport: USB CDC serial on the ItsyBitsy M4. Baud rate is ignored by native
  USB; the page opens at 115200.
- Framing: one ASCII command per line, terminated by `\n` (`\r\n` accepted).
  Commands are case-insensitive. Arguments are space-separated.
- Every command gets exactly one reply line. Streaming measurements are extra
  `M` lines that arrive on their own.

## Commands

| Command | Reply | What it does |
|---|---|---|
| `ID` | `ID LEDLAB <fw-version> <firmware-name> ItsyBitsyM4` | Identify. The page sends this first and refuses to proceed unless the reply starts with `ID LEDLAB`. |
| `LED R` / `LED A` / `LED G` / `LED B` | `OK LED R` | Select an LED. Runs the full sequence from the lab spec: all LED switches off, DAC to 0 V, then the chosen switch on. |
| `LED OFF` | `OK LED OFF` | All LED switches off, DAC to 0 V. |
| `SET <volts>` | `OK SET 1.650` | Set the drive voltage on A0 (DAC). Clamped to 0.000–3.300. Reply echoes the value actually set after clamping and DAC quantisation. |
| `MEAS` | `M <v_led> <i_amps> <v_res> <dac_v> <led>` | Run one CheckVoltage cycle (≈50 ms) and report. See the `M` line below. |
| `STREAM 1` / `STREAM 0` | `OK STREAM 1` | Start/stop continuous measurement. While on, an `M` line is emitted after every cycle (~10–15 Hz). |
| `R <ohms>` | `OK R 100.000` | Set the sense resistor value used for the current calculation. |
| `R` | `OK R 100.000` | Report the current sense resistor value. |
| `STATE` | `S <led> <dac_v> <r_ohms> <stream>` | Report current state without measuring. |
| `HELP` | several lines, last is `OK HELP` | Human-readable command list, for students poking at it from a terminal. |

Unknown command or bad argument: `ERR <reason>`.

## The `M` line

```
M 1.872 0.004120 0.412 2.300 R
  │     │        │     │     └─ selected LED: R A G B or OFF
  │     │        │     └─────── DAC output voltage, V
  │     │        └───────────── voltage across sense resistor, V
  │     └────────────────────── current through the LED, A  (= v_res / R)
  └──────────────────────────── voltage across the LED, V
```

Voltages have 3 decimals, current has 6. All values are in base units (V, A);
the page converts to mA for display.

## CheckVoltage cycle (what `MEAS` does)

Straight from the lab spec, implemented as a non-blocking state machine so
commands are still accepted during streaming:

1. D12 low, D11 high → A1 now reads the voltage across the LED
2. wait 25 ms (settling)
3. read A1 (16-sample average) → `v_led`
4. D11 low, D12 high → A1 now reads the voltage across the sense resistor
5. wait 25 ms
6. read A1 (16-sample average) → `v_res`; `i = v_res / R`
7. leave D11/D12 both low, emit `M`

## Pin map (ItsyBitsy M4)

| Pin | Direction | Role |
|---|---|---|
| A0 | DAC out | FET gate drive, 0–3.3 V |
| A1 | ADC in | Measurement node |
| D5 | out | Red LED switch (note: D5 on the ItsyBitsy M4 is output-only, 5 V logic through a level shifter) |
| D7 | out | Amber LED switch |
| D9 | out | Green LED switch |
| D10 | out | Blue LED switch |
| D11 | out | Route A1 to LED voltage |
| D12 | out | Route A1 to sense-resistor voltage |

## Safety behaviour

- On power-up: all switches low, DAC 0 V, streaming off.
- If the USB host disconnects (DTR drops) while an LED is selected, the
  firmware runs `LED OFF` itself. The page also sends `LED OFF` before it
  closes the port.
- Onboard red LED (D13) blinks once per second while streaming, so a board
  left running is visible from across the room.
