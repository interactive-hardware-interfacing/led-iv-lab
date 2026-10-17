// led_iv_lab.ino — LED I–V Lab firmware for the Adafruit ItsyBitsy M4
//
// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Interactive Hardware Interfacing contributors
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as published
// by the Free Software Foundation. See LICENSE in the repository root.
//
// Implements the serial protocol in PROTOCOL.md. The web page at
// https://interactive-hardware-interfacing.github.io/led-iv-lab/ talks to
// this over USB serial (Web Serial in Chrome/Edge). Any serial terminal
// works too — type HELP.
//
// Board:  Adafruit ItsyBitsy M4 (SAMD51)   — Tools > Board > Adafruit SAMD > ItsyBitsy M4
// Build:  Arduino IDE, or arduino-cli (see .github/workflows/firmware.yml)
// Flash:  double-tap RESET → drag the .uf2 onto the ITSYM4BOOT drive
//
// Pin roles come straight from the lab spec (AvrutinLabPseudoCode):
//   A0  DAC out  → FET gate (drive voltage)
//   A1  ADC in   ← measurement node
//   D5/D7/D9/D10 → Red/Amber/Green/Blue LED switches
//   D11 → route A1 to the LED voltage
//   D12 → route A1 to the sense-resistor voltage

#define FW_VERSION "1.0.0"
#define FW_NAME    "arduino"

// ---------------------------------------------------------------- pins
const int PIN_DAC     = A0;
const int PIN_SENSE   = A1;
const int PIN_LED_R   = 5;
const int PIN_LED_A   = 7;
const int PIN_LED_G   = 9;
const int PIN_LED_B   = 10;
const int PIN_SEL_LED = 11;   // A1 ← V across LED
const int PIN_SEL_RES = 12;   // A1 ← V across sense resistor
const int PIN_STATUS  = 13;   // onboard red LED

// ---------------------------------------------------------------- analog
// SAMD51 DAC and ADC are both referenced to 3.3 V here. If a meter shows
// the DAC full-scale is a little off, correct DAC_FULL_SCALE_V.
const float DAC_FULL_SCALE_V = 3.300f;
const float ADC_FULL_SCALE_V = 3.300f;
const int   DAC_BITS = 12;            // 0..4095
const int   ADC_BITS = 12;            // 0..4095
const int   ADC_OVERSAMPLE = 16;
const unsigned long SETTLE_MS = 25;   // from the spec: "Count 25 ms"

// ---------------------------------------------------------------- state
float  senseOhms   = 100.0f;          // placeholder; set with "R <ohms>"
float  dacVolts    = 0.0f;
char   ledSel      = 'X';             // 'R','A','G','B' or 'X' (off)
bool   streaming   = false;
bool   hostWasConnected = false;

// CheckVoltage state machine
enum MeasPhase { IDLE, SETTLE_LED, SETTLE_RES };
MeasPhase     phase = IDLE;
unsigned long phaseStart = 0;
float         vLed = 0.0f, vRes = 0.0f;
bool          oneShot = false;        // true when a MEAS (not stream) is pending

// serial line buffer
char   lineBuf[64];
size_t lineLen = 0;

// ---------------------------------------------------------------- helpers
void setDac(float volts) {
  if (volts < 0.0f) volts = 0.0f;
  if (volts > DAC_FULL_SCALE_V) volts = DAC_FULL_SCALE_V;
  int code = (int)(volts / DAC_FULL_SCALE_V * ((1 << DAC_BITS) - 1) + 0.5f);
  analogWrite(PIN_DAC, code);
  dacVolts = (float)code * DAC_FULL_SCALE_V / ((1 << DAC_BITS) - 1);
}

float readSense() {
  long acc = 0;
  for (int i = 0; i < ADC_OVERSAMPLE; i++) acc += analogRead(PIN_SENSE);
  float code = (float)acc / ADC_OVERSAMPLE;
  return code * ADC_FULL_SCALE_V / ((1 << ADC_BITS) - 1);
}

void allLedSwitchesOff() {
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_A, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, LOW);
}

// The sequence from the spec: everything off, DAC to 0, then the one switch on.
void selectLed(char which) {
  allLedSwitchesOff();
  setDac(0.0f);
  switch (which) {
    case 'R': digitalWrite(PIN_LED_R, HIGH); break;
    case 'A': digitalWrite(PIN_LED_A, HIGH); break;
    case 'G': digitalWrite(PIN_LED_G, HIGH); break;
    case 'B': digitalWrite(PIN_LED_B, HIGH); break;
    default:  which = 'X'; break;
  }
  ledSel = which;
}

const char* ledName(char c) {
  switch (c) { case 'R': return "R"; case 'A': return "A";
               case 'G': return "G"; case 'B': return "B"; default: return "OFF"; }
}

void emitMeasurement() {
  float amps = (senseOhms > 0.0f) ? vRes / senseOhms : 0.0f;
  Serial.print("M ");
  Serial.print(vLed, 3);  Serial.print(' ');
  Serial.print(amps, 6);  Serial.print(' ');
  Serial.print(vRes, 3);  Serial.print(' ');
  Serial.print(dacVolts, 3); Serial.print(' ');
  Serial.println(ledName(ledSel));
}

void startMeasurement() {
  digitalWrite(PIN_SEL_RES, LOW);
  digitalWrite(PIN_SEL_LED, HIGH);
  phase = SETTLE_LED;
  phaseStart = millis();
}

// Non-blocking CheckVoltage. Call every loop().
void serviceMeasurement() {
  unsigned long now = millis();
  switch (phase) {
    case IDLE:
      if (streaming || oneShot) startMeasurement();
      break;
    case SETTLE_LED:
      if (now - phaseStart >= SETTLE_MS) {
        vLed = readSense();
        digitalWrite(PIN_SEL_LED, LOW);
        digitalWrite(PIN_SEL_RES, HIGH);
        phase = SETTLE_RES;
        phaseStart = now;
      }
      break;
    case SETTLE_RES:
      if (now - phaseStart >= SETTLE_MS) {
        vRes = readSense();
        digitalWrite(PIN_SEL_RES, LOW);
        emitMeasurement();
        oneShot = false;
        phase = IDLE;
      }
      break;
  }
}

// ---------------------------------------------------------------- commands
void printHelp() {
  Serial.println("LED I-V Lab  (" FW_NAME " " FW_VERSION ")");
  Serial.println("  ID            identify");
  Serial.println("  LED R|A|G|B   select LED (resets drive to 0 V)");
  Serial.println("  LED OFF       all LEDs off, drive 0 V");
  Serial.println("  SET <volts>   drive voltage 0..3.3");
  Serial.println("  MEAS          one measurement -> M <v_led> <i_A> <v_res> <dac_V> <led>");
  Serial.println("  STREAM 1|0    continuous measurements on/off");
  Serial.println("  R [<ohms>]    get/set sense resistor");
  Serial.println("  STATE         report state");
  Serial.println("OK HELP");
}

void handleLine(char* line) {
  // uppercase in place, split on first space
  for (char* p = line; *p; ++p) *p = toupper((unsigned char)*p);
  char* arg = strchr(line, ' ');
  if (arg) { *arg = '\0'; ++arg; while (*arg == ' ') ++arg; }

  if (!strcmp(line, "ID")) {
    Serial.println("ID LEDLAB " FW_VERSION " " FW_NAME " ItsyBitsyM4");
  }
  else if (!strcmp(line, "LED")) {
    if (!arg) { Serial.println("ERR LED needs R|A|G|B|OFF"); return; }
    if (!strcmp(arg, "OFF")) { selectLed('X'); Serial.println("OK LED OFF"); return; }
    if (strlen(arg) == 1 && strchr("RAGB", arg[0])) {
      selectLed(arg[0]);
      Serial.print("OK LED "); Serial.println(arg[0]);
    } else Serial.println("ERR LED needs R|A|G|B|OFF");
  }
  else if (!strcmp(line, "SET")) {
    if (!arg) { Serial.println("ERR SET needs volts"); return; }
    setDac(atof(arg));
    Serial.print("OK SET "); Serial.println(dacVolts, 3);
  }
  else if (!strcmp(line, "MEAS")) {
    oneShot = true;   // reply is the M line itself
  }
  else if (!strcmp(line, "STREAM")) {
    if (!arg) { Serial.println("ERR STREAM needs 1|0"); return; }
    streaming = (arg[0] == '1');
    Serial.print("OK STREAM "); Serial.println(streaming ? 1 : 0);
  }
  else if (!strcmp(line, "R")) {
    if (arg) {
      float r = atof(arg);
      if (r <= 0.0f) { Serial.println("ERR R must be > 0"); return; }
      senseOhms = r;
    }
    Serial.print("OK R "); Serial.println(senseOhms, 3);
  }
  else if (!strcmp(line, "STATE")) {
    Serial.print("S "); Serial.print(ledName(ledSel)); Serial.print(' ');
    Serial.print(dacVolts, 3); Serial.print(' ');
    Serial.print(senseOhms, 3); Serial.print(' ');
    Serial.println(streaming ? 1 : 0);
  }
  else if (!strcmp(line, "HELP") || !strcmp(line, "?")) {
    printHelp();
  }
  else if (line[0] == '\0') {
    // blank line: ignore
  }
  else {
    Serial.print("ERR unknown command "); Serial.println(line);
  }
}

void serviceSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) { lineBuf[lineLen] = '\0'; handleLine(lineBuf); lineLen = 0; }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0;  // overflow: drop the line
      Serial.println("ERR line too long");
    }
  }
}

// If the host goes away (DTR drops) while an LED is driven, shut it down.
void serviceHostLink() {
  bool connected = (bool)Serial;
  if (hostWasConnected && !connected) {
    streaming = false;
    selectLed('X');
  }
  hostWasConnected = connected;
}

void serviceStatusLed() {
  static unsigned long last = 0;
  static bool on = false;
  unsigned long now = millis();
  if (streaming) {
    if (now - last >= 500) { on = !on; digitalWrite(PIN_STATUS, on); last = now; }
  } else if (on) {
    on = false; digitalWrite(PIN_STATUS, LOW);
  }
}

// ---------------------------------------------------------------- arduino
void setup() {
  pinMode(PIN_LED_R, OUTPUT);  pinMode(PIN_LED_A, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_SEL_LED, OUTPUT); pinMode(PIN_SEL_RES, OUTPUT);
  pinMode(PIN_STATUS, OUTPUT);
  allLedSwitchesOff();
  digitalWrite(PIN_SEL_LED, LOW);
  digitalWrite(PIN_SEL_RES, LOW);

  analogWriteResolution(DAC_BITS);
  analogReadResolution(ADC_BITS);
  analogReference(AR_DEFAULT);          // 3.3 V
  setDac(0.0f);

  Serial.begin(115200);                 // native USB: baud is ignored
  // Deliberately not waiting for Serial — the board must be safe with no host.
}

void loop() {
  serviceHostLink();
  serviceSerial();
  serviceMeasurement();
  serviceStatusLed();
}
