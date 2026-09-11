<!--
  SPLASH IMAGE - REQUIRED, and the first element of the README.
  Build it from one of the blank banner templates in the Arduino Library HowTo repo
  (docs/images/gh-banner-simple-qw-ard.png), add the repo name on the left and a product
  photo + board tile in the white area, save it here as docs/images/gh-banner-gm-x02b.png,
  and upload the same image as the repo's Social preview under Settings > General.
-->
![SparkFun Qwiic MEMS Gas Sensors - GM-X02B](docs/images/gh-banner-gm-x02b.png "SparkFun Qwiic MEMS Gas Sensors - GM-X02B")

# SparkFun Qwiic MEMS Gas Sensors - GM-X02B

Arduino Library for the SparkFun Qwiic MEMS gas sensor breakouts, covering all nine Winsen GM-x02B sensing elements.

![GitHub License](https://img.shields.io/github/license/sparkfun/SparkFun_GM-X02B_Arduino_Library)
![Release](https://img.shields.io/github/v/release/sparkfun/SparkFun_GM-X02B_Arduino_Library)
![Release Date](https://img.shields.io/github/release-date/sparkfun/SparkFun_GM-X02B_Arduino_Library)
![Documentation - build](https://img.shields.io/github/actions/workflow/status/sparkfun/SparkFun_GM-X02B_Arduino_Library/build-deploy-ghpages.yml?label=doc%20build)
[![Compile Test](https://github.com/sparkfun/SparkFun_GM-X02B_Arduino_Library/actions/workflows/test-compile-sketch.yml/badge.svg)](https://github.com/sparkfun/SparkFun_GM-X02B_Arduino_Library/actions/workflows/test-compile-sketch.yml)
![GitHub issues](https://img.shields.io/github/issues/sparkfun/SparkFun_GM-X02B_Arduino_Library)

## Library Description

The SparkFun Qwiic MEMS gas sensor boards put Winsen's GM-x02B metal-oxide semiconductor (MOS) gas sensing elements on Qwiic-enabled breakouts. Each board pairs one sensing element with a Texas Instruments ADC101C021 10-bit I2C ADC, so gas readings arrive straight over Qwiic - no external ADC required.

All nine variants share one board design and one library. Pick your gas:

| Board | Class | Target gas | Range | Default address |
| -- | -- | -- | -- | -- |
| **GM-102B** | `SfeGM102BArdI2C` | Nitrogen dioxide (NO<sub>2</sub>) | 0.1 - 10 ppm | `0x50` |
| **GM-202B** | `SfeGM202BArdI2C` | Alcohol, smoke | 10 - 1000 ppm | `0x51` |
| **GM-302B** | `SfeGM302BArdI2C` | Ethanol | 1 - 500 ppm | `0x52` |
| **GM-402B** | `SfeGM402BArdI2C` | Methane, propane | 1 - 10000 ppm | `0x54` |
| **GM-502B** | `SfeGM502BArdI2C` | VOCs | 1 - 500 ppm | `0x55` |
| **GM-512B** | `SfeGM512BArdI2C` | Breath odor (H<sub>2</sub>S) | 0.5 - 50 ppm | `0x56` |
| **GM-602B** | `SfeGM602BArdI2C` | Hydrogen sulfide (H<sub>2</sub>S) | 0.5 - 50 ppm | `0x58` |
| **GM-702B** | `SfeGM702BArdI2C` | Carbon monoxide (CO) | 5 - 5000 ppm | `0x59` |
| **GM-802B** | `SfeGM802BArdI2C` | Ammonia (NH<sub>3</sub>) | 1 - 300 ppm | `0x5A` |

Built on the [SparkFun Toolkit](https://github.com/sparkfun/SparkFun_Toolkit).

## Functionality

- Raw 10-bit ADC readings, output voltage, and sensing element resistance (Rs)
- Clean-air calibration (measure and store R0, or restore a saved value)
- **Direction-normalized gas response** — one number that means the same thing on all nine parts
- Optional user-fitted ppm conversion, plus per-part detection ranges
- Hardware gas threshold alerts with programmable limits and hysteresis
- ALERT pin support (open-drain, configurable polarity and hold behavior)
- Automatic conversion mode — the ADC monitors the sensor entirely on its own
- Hardware peak gas tracking, mapped to the correct extremum register per part
- Multiple boards on one bus, mixing gases freely

## The one thing to know: the GM-102B runs backwards

Eight of the nine sensing elements detect **reducing** gases. Their resistance **falls** as gas concentration rises, so ADC counts rise.

The GM-102B detects NO₂, an **oxidizing** gas. Its resistance **rises** with concentration, so its ADC counts *fall*. Its datasheet says so directly — `R0(air)/Rs(2ppm NO2) ≤ 0.5`, where every other part specifies `≥ 2` or `≥ 3`.

Because Rs sits on the high side of the load-resistor divider, that inversion propagates through everything: which direction the counts move, which alert limit means "gas detected", and which hardware min/max register holds the peak gas reading.

**This library handles it for you.** Use `getGasResponse()` instead of `getRatio()`, `setGasAlertResponse()` instead of the raw limit registers, `getGasAlert()` instead of `getAlertStatus()`, and `getPeakGasCounts()` instead of `getHighestConversion()`. Those four are direction-aware, so one sketch works on any board in the family. The raw register accessors are all still there — they're direction-*neutral* by definition, and `rsRisesWithGas()` tells you which way your part goes if you want to do the mapping yourself.

## Hardware Connections

The boards connect over I2C using the Qwiic connector — no soldering required. Each variant ships at a unique default address (set by the tri-level ADR1 / ADR0 jumpers), so multiple boards coexist on one bus. The PTH header additionally breaks out the raw analog VOUT and the ADC's ALERT output.

> [!WARNING]
> **No pull-up is fitted on ALERT.** The ADC101C021's ALERT is an open-drain output in *both* polarity modes, so the pin floats unless something pulls it high. For a single board the MCU's internal pull-up is enough — `pinMode(alertPin, INPUT_PULLUP)` — because ALERT leaks at most 1 µA when high-impedance, so even a weak 50 kΩ internal pull-up droops only 50 mV. Fit a discrete **10 kΩ to 3.3 V** when your MCU pin has no internal pull-up, when several boards share one alert line, or when the wire is long: that's 330 µA of sink current (well inside the 3 mA V<sub>OL</sub> spec), 10 mV of droop, and a ~0.2 µs edge. Keep it between 1 kΩ and 100 kΩ. You don't need the pin at all to use the alert function — `getGasAlert()` reads the same condition over I2C. Example03 shows both.

One board-level difference worth knowing: **the GM-102B carries a 68.1 kΩ load resistor** where the rest of the family uses 4.7 kΩ. Its resistance decade sits two orders higher than the other parts', and with 4.7 kΩ a unit at the high end of the manufacturing spread produces under five ADC counts of usable swing. The library reads the correct value from its per-part table, so you never set it by hand.

> [!IMPORTANT]
> These are heated MOS sensors and must be preheated before use: allow a few minutes for the output to stabilize after power-up, and after long unpowered storage Winsen recommends 24–168 hours of powered aging depending on the part (see its datasheet). Out of the box these boards are good for **relative** measurement and threshold alerts; **absolute ppm requires calibration** against known gas concentrations. Never expose the sensing element to organic silicon vapors, and see the datasheet cautions for other handling notes.

## Using the Library

### Installation

Install through the Arduino Library Manager by searching for **SparkFun Qwiic Gas Sensor**, or download this repository as a ZIP and add it via *Sketch > Include Library > Add .ZIP Library*. This library depends on the [SparkFun Toolkit](https://github.com/sparkfun/SparkFun_Toolkit), which the Library Manager will offer to install alongside it.

### Getting Started

Declare which board you have by picking its class. There is one per variant, so the board identity lives in the type — nothing to pass, nothing to get wrong:

```c++
#include <SparkFun_GMX02B.h>

// Uncomment the board you're using
// SfeGM102BArdI2C mySensor;   // GM-102B - nitrogen dioxide (NO2)
// SfeGM202BArdI2C mySensor;   // GM-202B - alcohol, smoke
// SfeGM302BArdI2C mySensor;   // GM-302B - ethanol
// SfeGM402BArdI2C mySensor;   // GM-402B - methane, propane
// SfeGM502BArdI2C mySensor;   // GM-502B - VOCs
// SfeGM512BArdI2C mySensor;   // GM-512B - breath odor
SfeGM602BArdI2C mySensor;      // GM-602B - hydrogen sulfide (H2S)
// SfeGM702BArdI2C mySensor;   // GM-702B - carbon monoxide (CO)
// SfeGM802BArdI2C mySensor;   // GM-802B - ammonia (NH3)
```

The library can't read the sensing element back from the hardware — the ADC101C021 has no ID register — and getting it wrong silently inverts the GM-102B's readings and picks the wrong load resistor. So it has to be declared somewhere, and the type is the safest place for it.

If you need to pick the part at runtime, or hold several boards in one array, use the generic class with a **required** part argument (there is deliberately no default):

```c++
SfeGMX02BArdI2C mySensor(GM_602B);
```

In `setup()`, start I2C and call `begin()`:

```c++
Wire.begin();

while (mySensor.begin() == false)   // this part's factory address, 0x58
{
    Serial.println("Gas sensor not connected, check your wiring!");
    delay(1000);
}
```

### Declaring the part vs. guessing it

Sensor identity and I2C address are two different facts, and this library keeps them separate on purpose. The **part** is declared; the **address** defaults from it but can be overridden:

```c++
mySensor.begin();          // factory address for the declared part
mySensor.begin(0x51);      // jumpers moved — same sensor, new address, correct math
```

That's what makes a re-addressed board safe. If identity rode on the address, moving an ADR jumper to dodge a bus conflict could silently turn a GM-102B into a "GM-602B" and invert every reading, with nothing to signal it.

`beginAutoDetect()` is still available for a single-board demo, and it reports honestly:

```c++
if (mySensor.beginAutoDetect())
{
    Serial.print("Found something at 0x");
    Serial.println(mySensor.getAddress(), HEX);

    if (mySensor.isIdentityAssumed())
        Serial.println("...sensor type ASSUMED from the address, not measured.");
}
```

> [!WARNING]
> Address-based detection is a guess, and the guess picks the curve direction. It also can't distinguish a gas sensor from anything else on those addresses — `0x50`–`0x57` is the classic 24-series EEPROM block, and SparkFun's own Qwiic EEPROM defaults to `0x50`. `probeADC()` adds a read-only check that the responding registers look like an ADC101C021 (it never writes, precisely because the device might *be* an EEPROM), but a zeroed EEPROM would still pass. Declare the part for anything real.

To list what's actually on the bus without claiming to identify it:

```c++
uint8_t found[9];
uint8_t count = SfeGMX02BArdI2C::scanFamily(found, sizeof(found));
```

And if `begin()` fails, `findMisplacedBoard()` diagnoses the likeliest cause — it returns the address of the single other family board responding, if there's exactly one.

### Using several of the same sensor

Nothing in this library ties a part number to an address, so several boards of the *same* type are no different from several boards of different types. Give each one its own object and its own address:

```c++
// Three H2S boards. Two have had their ADR jumpers moved off the factory 0x58.
SfeGM602BArdI2C intake;
SfeGM602BArdI2C exhaust;
SfeGM602BArdI2C ambient;

intake.begin();          // 0x58, the GM-602B factory address
exhaust.begin(0x50);     // re-jumpered
ambient.begin(0x55);     // re-jumpered
```

All three keep the GM-602B's curve direction, its 4.7 k&#8486; load resistor, and its detection range, because those come from the **declared part**, not the address. Each object holds its own bus handle, its own cached reading, and its own R0 — the const part table is shared but read-only, so instances never interfere.

**Any board can take any of the nine addresses.** The two tri-state jumpers give 3 &times; 3 = 9 combinations, and each variant just *ships* on a different one so that a one-of-each bus works out of the box. Re-jumper freely; `sfe_gm_familyAddresses()` returns the nine valid values, and the decode table is in the comment block at the top of `sfDevGMX02B.cpp`.

Three consequences worth knowing:

> [!WARNING]
> **Never use `beginAutoDetect()` with re-addressed boards.** It infers the part from the address, so three GM-602Bs at 0x58 / 0x50 / 0x55 would be reported as a GM-602B, a GM-102B and a GM-502B — and the "GM-102B" would get an inverted curve and a 68.1 k&#8486; load resistor. This isn't a corner case here, it's guaranteed. Auto-detect is for a single board on its factory address.

- **Nine boards per bus is the ceiling**, whatever the mix of types, because there are only nine addresses. For more, use a second I2C port — `begin()` takes a `TwoWire&`, so `fourth.begin(0x58, Wire1)` puts another nine on `Wire1`.
- **Each unit needs its own clean-air calibration.** R0 belongs to an individual sensing element, not to a part number, and Winsen's part-to-part spread is on the order of 10:1 — so two GM-602Bs sitting side by side in the same clean air will have genuinely different R0 values. Call `measureCleanAirResistance()` per object.
- **The library can't detect an address collision.** If you forget to re-jumper one board and two sit at 0x58, both ACK and you get contention, not an error. Sanity-check the bus before you trust it:

```c++
uint8_t found[9];
uint8_t count = SfeGMX02BArdI2C::scanFamily(found, sizeof(found));

if (count != 3)
    Serial.println("Expected 3 boards - check the ADR jumpers for duplicates.");
```

### Reading the Sensor

One `readMeasurement()` call fetches a conversion; the getters then return it in whichever form you need:

Following the SparkFun Toolkit convention, getters that can fail return an `sfTkError_t` and hand back the value through an output reference:

```c++
float ohms;

mySensor.readMeasurement();
mySensor.getSensorResistance_Ohm(ohms);

Serial.print("Raw: ");
Serial.print(mySensor.getRaw());           // 0 - 1023
Serial.print("  Voltage: ");
Serial.print(mySensor.getVoltage_V(), 3);  // volts across the load resistor
Serial.print("  Rs: ");
Serial.println(ohms, 0);                   // ohms
```

`getRaw()` and `getVoltage_V()` return directly because they read the cached conversion and cannot fail. `getSensorResistance_Ohm()`, `getRatio()`, `getGasResponse()` and `getConcentration_ppm()` return an `sfTkError_t`, because each has a real failure mode — no clean-air reference, an off-scale reading, no fitted curve. Example03 shows the checked form; the other examples skip it, since once `begin()` succeeds the rest is reliable.

### Clean-Air Response

Calibrate once in clean air with the sensor warmed up, then track the gas response — near **1.0 in clean air and rising with gas on every part in the family**:

```c++
float r0;
mySensor.measureCleanAirResistance(r0);   // averages 16 readings and stores R0

float response;
mySensor.readMeasurement();
mySensor.getGasResponse(response);
```

Because the response is normalized in the same direction the datasheets quote sensitivity, it compares directly against `getRatedSensitivity()`:

| Part | Datasheet spec | Equivalent test |
| -- | -- | -- |
| GM-602B | `R0/Rs ≥ 3` in 50 ppm H₂S | `getGasResponse() >= 3.0` |
| GM-102B | `R0/Rs ≤ 0.5` in 2 ppm NO₂ | `getGasResponse() >= 2.0` |

`getRatio()` still returns the raw physical Rs/R0 if you want to plot against a datasheet curve.

### Hardware Alerts

The ADC can watch the sensor by itself. Set a threshold in response units, enable automatic conversion mode, and poll the flag (or wire the ALERT pin to an interrupt):

```c++
mySensor.setGasAlertResponse(2.0);            // twice the clean-air response
mySensor.setAlertHysteresis_mV(32.0);         // in mV of sensor output, not ADC counts
mySensor.setCycleTime(GM_CYCLE_TIME_2048);    // ADC free-runs

bool gasDetected;
mySensor.getGasAlert(gasDetected);
if (gasDetected)
    Serial.println("Gas threshold exceeded!");
```

`setGasAlertResponse()` writes the high limit on a falling-Rs part and the low limit on the GM-102B; `getGasAlert()` maps the status flags back the same way.

### Concentration in ppm

This library ships **no** conversion curves, and that's deliberate. Winsen publishes no numeric curve data for any GM-x02B part — the datasheet sensitivity plots are raster images with no axis values — and the GM-602B manual describes the response as "non linear, hysteretic and non-repetitive." Any coefficient table here would be invented numbers wearing a datasheet's authority.

If you characterize a sensor against known gas concentrations, fit a power law and hand it over:

```c++
mySensor.setResponseCurve(a, b);          // ppm = a * pow(gas response, b)

float ppm;
if (mySensor.getConcentration_ppm(ppm) != ksfTkErrOk)
    Serial.println("No curve fitted for this sensor yet");
else if (!mySensor.isInRange(ppm))
    Serial.println("Outside this part's rated detection range");
```

## Examples

- **Example01_BasicReadings** — raw counts, voltage, and sensor resistance once per second
- **Example02_CleanAirResponse** — clean-air calibration and direction-normalized response
- **Example03_GasAlertThreshold** — hardware gas threshold alert using automatic conversion mode
- **Example04_PeakTracking** — hardware peak gas tracking over a rolling window
- **Example05_FindBoards** — scan the bus, and the honest limits of address-based detection
- **Example06_MultipleSensors** — three gases on one bus, including the inverted GM-102B (see [Using several of the same sensor](#using-several-of-the-same-sensor) for identical boards at custom addresses)

## Repository Contents

* **/.github/workflows** - GitHub Action workflows for the compile test and documentation build.
* **/docs** - Configuration files for the documentation.
* **/examples** - Example sketches for the library (.ino). Run these from the Arduino IDE.
* **/src** - Source files for the library (.cpp, .h).
* **keywords.txt** - Keywords from this library that will be highlighted in the Arduino IDE.
* **library.properties** - General library properties for the Arduino package manager.

## Documentation

* **[Library Documentation](https://docs.sparkfun.com/SparkFun_GM-X02B_Arduino_Library/)** - Doxygen-generated API reference for this library.
* **[Hookup Guide](https://docs.sparkfun.com/SparkFun_Qwiic_MEMS_Hydrogen_Sulfide_Sensor_GM-602B/)** - Hookup guide, including preheating, calibration, and handling cautions.
* **[Installing an Arduino Library Guide](https://learn.sparkfun.com/tutorials/installing-an-arduino-library)** - Basic information on how to install an Arduino library.
* **[Hardware Repository](https://github.com/sparkfun/SparkFun_Qwiic_Gas_Sensor_GM-602B)** - Design files for the board.

## Products That Use This Library

| Product ID | Description |
| -- | -- |
| TODO | SparkFun Qwiic MEMS Nitrogen Dioxide Sensor - GM-102B |
| TODO | SparkFun Qwiic MEMS Alcohol / Smoke Sensor - GM-202B |
| TODO | SparkFun Qwiic MEMS Ethanol Sensor - GM-302B |
| TODO | SparkFun Qwiic MEMS Combustible Gas Sensor - GM-402B |
| TODO | SparkFun Qwiic MEMS VOC Sensor - GM-502B |
| TODO | SparkFun Qwiic MEMS Breath Odor Sensor - GM-512B |
| TODO | SparkFun Qwiic MEMS Hydrogen Sulfide Sensor - GM-602B |
| TODO | SparkFun Qwiic MEMS Carbon Monoxide Sensor - GM-702B |
| TODO | SparkFun Qwiic MEMS Ammonia Sensor - GM-802B |

## License Information

This product is ***open source***!

This product is licensed using the [MIT Open Source License](https://opensource.org/license/mit)
