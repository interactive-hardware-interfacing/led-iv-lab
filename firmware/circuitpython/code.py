# code.py -- LED I-V Lab firmware for the Adafruit ItsyBitsy M4 (CircuitPython)
#
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Interactive Hardware Interfacing contributors
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation. See LICENSE in the repository root.
#
# Same serial protocol as the Arduino firmware (see PROTOCOL.md), so the web
# page works with either. Flash CircuitPython 9.x for the ItsyBitsy M4, then
# copy this file to the CIRCUITPY drive as code.py.
#
# Timing note: CircuitPython has more jitter than the Arduino build. The 25 ms
# settle is a minimum here, not an exact figure; measurement rate is lower.

import time
import board
import analogio
import digitalio
import supervisor
import usb_cdc

FW_VERSION = "1.0.0"
FW_NAME = "circuitpython"

DAC_FULL_SCALE_V = 3.3
ADC_FULL_SCALE_V = 3.3
ADC_OVERSAMPLE = 16
SETTLE_S = 0.025

# ---------------------------------------------------------------- pins
dac = analogio.AnalogOut(board.A0)
adc = analogio.AnalogIn(board.A1)

def _out(pin):
    d = digitalio.DigitalInOut(pin)
    d.direction = digitalio.Direction.OUTPUT
    d.value = False
    return d

led_sw = {"R": _out(board.D5), "A": _out(board.D7),
          "G": _out(board.D9), "B": _out(board.D10)}
sel_led = _out(board.D11)
sel_res = _out(board.D12)
status = _out(board.D13)

# ---------------------------------------------------------------- state
sense_ohms = 100.0
dac_volts = 0.0
led_sel = "OFF"
streaming = False
host_was_connected = False

# ---------------------------------------------------------------- helpers
def set_dac(volts):
    global dac_volts
    volts = min(max(volts, 0.0), DAC_FULL_SCALE_V)
    code = int(volts / DAC_FULL_SCALE_V * 65535 + 0.5)   # AnalogOut is 16-bit scaled
    dac.value = code
    dac_volts = code * DAC_FULL_SCALE_V / 65535

def read_sense():
    acc = 0
    for _ in range(ADC_OVERSAMPLE):
        acc += adc.value
    return (acc / ADC_OVERSAMPLE) * ADC_FULL_SCALE_V / 65535

def select_led(which):
    global led_sel
    for sw in led_sw.values():
        sw.value = False
    set_dac(0.0)
    if which in led_sw:
        led_sw[which].value = True
        led_sel = which
    else:
        led_sel = "OFF"

def measure():
    """Blocking CheckVoltage cycle (~50 ms). Returns the M line."""
    sel_res.value = False
    sel_led.value = True
    time.sleep(SETTLE_S)
    v_led = read_sense()
    sel_led.value = False
    sel_res.value = True
    time.sleep(SETTLE_S)
    v_res = read_sense()
    sel_res.value = False
    amps = v_res / sense_ohms if sense_ohms > 0 else 0.0
    return "M %.3f %.6f %.3f %.3f %s" % (v_led, amps, v_res, dac_volts, led_sel)

def send(line):
    usb_cdc.console.write((line + "\n").encode())

HELP = (
    "LED I-V Lab  (%s %s)\n"
    "  ID            identify\n"
    "  LED R|A|G|B   select LED (resets drive to 0 V)\n"
    "  LED OFF       all LEDs off, drive 0 V\n"
    "  SET <volts>   drive voltage 0..3.3\n"
    "  MEAS          one measurement -> M <v_led> <i_A> <v_res> <dac_V> <led>\n"
    "  STREAM 1|0    continuous measurements on/off\n"
    "  R [<ohms>]    get/set sense resistor\n"
    "  STATE         report state\n"
    "OK HELP" % (FW_NAME, FW_VERSION)
)

def handle(line):
    global streaming, sense_ohms
    parts = line.strip().upper().split()
    if not parts:
        return
    cmd, args = parts[0], parts[1:]

    if cmd == "ID":
        send("ID LEDLAB %s %s ItsyBitsyM4" % (FW_VERSION, FW_NAME))
    elif cmd == "LED":
        if not args or (args[0] not in led_sw and args[0] != "OFF"):
            send("ERR LED needs R|A|G|B|OFF"); return
        select_led(args[0])
        send("OK LED " + led_sel)
    elif cmd == "SET":
        try:
            set_dac(float(args[0]))
        except (IndexError, ValueError):
            send("ERR SET needs volts"); return
        send("OK SET %.3f" % dac_volts)
    elif cmd == "MEAS":
        send(measure())
    elif cmd == "STREAM":
        if not args:
            send("ERR STREAM needs 1|0"); return
        streaming = args[0] == "1"
        send("OK STREAM %d" % (1 if streaming else 0))
    elif cmd == "R":
        if args:
            try:
                r = float(args[0])
            except ValueError:
                send("ERR R must be > 0"); return
            if r <= 0:
                send("ERR R must be > 0"); return
            sense_ohms = r
        send("OK R %.3f" % sense_ohms)
    elif cmd == "STATE":
        send("S %s %.3f %.3f %d" % (led_sel, dac_volts, sense_ohms, 1 if streaming else 0))
    elif cmd in ("HELP", "?"):
        send(HELP)
    else:
        send("ERR unknown command " + cmd)

# ---------------------------------------------------------------- main loop
# usb_cdc.console is the same port the REPL would use; the REPL only takes
# over if code.py exits, so stay in the loop.
console = usb_cdc.console
console.timeout = 0
buf = b""
last_blink = time.monotonic()
blink_on = False

select_led("OFF")

while True:
    connected = console.connected
    if host_was_connected and not connected:
        streaming = False
        select_led("OFF")
    host_was_connected = connected

    n = console.in_waiting
    if n:
        buf += console.read(n)
        while b"\n" in buf or b"\r" in buf:
            for sep in (b"\n", b"\r"):
                if sep in buf:
                    line, buf = buf.split(sep, 1)
                    break
            try:
                handle(line.decode())
            except Exception as e:   # keep the instrument alive on bad input
                send("ERR %s" % e)

    if streaming:
        send(measure())
        now = time.monotonic()
        if now - last_blink >= 0.5:
            blink_on = not blink_on
            status.value = blink_on
            last_blink = now
    elif blink_on:
        blink_on = False
        status.value = False
