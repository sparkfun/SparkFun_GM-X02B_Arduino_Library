/*
  Example 03 - Gas Alert Threshold

  Hardware gas threshold detection with no polling. The onboard ADC101C021 can watch the sensor
  entirely by itself: in automatic conversion mode it converts on its own schedule, compares every
  result against a programmable limit, and raises a flag (and optionally its ALERT pin) when the
  limit is crossed.

  setGasAlertResponse() takes a threshold in the same units as getGasResponse() - 1.0 is clean air,
  larger means more gas - and writes it to whichever limit register means "more gas" for your part.
  That matters: on the GM-102B, resistance RISES with NO2, so ADC counts FALL and the gas-side trip
  point is the LOW limit, not the high one. getGasAlert() maps the status flags back the same way.
  Neither call requires your sketch to know which direction its sensor moves.

  If you want to work in raw ADC counts instead, setAlertLowLimit() / setAlertHighLimit() /
  getAlertStatus() are still there and are direction-neutral - but then the direction is yours to
  handle. rsRisesWithGas() tells you which way this part goes.

  This is also the one example that checks return codes on every call. The others skip it: once
  begin() has succeeded, the rest is reliable enough that error handling is just clutter. Here it
  earns its place, because a threshold set against a missing clean-air reference would silently
  never fire.

  SparkFun Electronics
  Date: 2026
  SparkFun code, firmware, and software is released under the MIT License.
    Please see LICENSE.md for further details.

  Hardware Connections:
  IoT RedBoard --> GM-x02B Breakout
  QWIIC --> QWIIC

  By default this sketch polls the alert over I2C with getGasAlert(), which needs no extra wiring
  at all. Uncomment USE_ALERT_PIN below to read the ADC's hardware ALERT pin instead.

  IMPORTANT - THE BOARD DOES NOT POPULATE A PULL-UP ON ALERT. The ADC101C021's ALERT is an open
  drain output in BOTH polarity modes (setAlertPinActiveHigh() changes only the logic sense, not
  the output structure), so nothing holds it high on its own. Without a pull-up the pin floats and
  reads as garbage. The sketch enables the MCU's internal pull-up for you; see the notes at
  kAlertPin for when to fit a discrete resistor instead, and what value to use.

  Open the Serial Monitor at 115200 baud.

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/
*/

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

// Trip when the response reaches twice its clean-air value. Tune this for your application -
// getRatedSensitivity() is a reasonable upper anchor (the response the part is specified to
// reach at getRatedConcentration_ppm()).
const float kAlertResponse = 2.0;

// Hysteresis, so the alert does not chatter right at the threshold. Set in millivolts of sensor
// output rather than raw ADC counts - setAlertHysteresis_mV() does the conversion.
const float kHysteresis_mV = 32.0;

// ---------------------------------------------------------------------------------------------
// Optional: use the ADC's hardware ALERT pin instead of polling over I2C.
// Uncomment to enable, and wire the board's ALERT PTH pad to the GPIO named below.
// ---------------------------------------------------------------------------------------------
// #define USE_ALERT_PIN

#ifdef USE_ALERT_PIN

// GPIO wired to the board's ALERT pad. Change to a pin your board actually has.
const uint8_t kAlertPin = 2;

// ALERT is open drain and the board has NO pull-up fitted, so one has to come from somewhere.
// The MCU's internal pull-up is enough for a single board on a short wire:
//
//   ALERT leaks at most 1 uA when high-impedance (IOZ, datasheet), so even a weak 50 kOhm
//   internal pull-up droops only 50 kOhm x 1 uA = 50 mV below the rail - still a solid logic high.
//   Pulling low is never the problem: VOL is 0.4 V max at 3 mA, and the internal pull-up sources
//   only 3.3 V / 50 kOhm = 66 uA.
//
// Set this false and fit a discrete resistor from ALERT to 3.3V instead when your MCU pin has no
// internal pull-up, when several boards share one alert line, or when the wire is long enough that
// the RC edge matters.
//
//   USE 10 kOhm.  Sink current 3.3 V / 10 kOhm = 330 uA (well inside the 3 mA VOL spec), droop
//                 10 kOhm x 1 uA = 10 mV, and an edge of 10 kOhm x ~20 pF = 0.2 us.
//   Floor  1 kOhm - 3.3 mA sink, already past the 0.4 V VOL test point.
//   Ceiling 100 kOhm - 100 mV of leakage droop and a ~2 us edge.
//
const bool kUseInternalPullup = true;

#endif // USE_ALERT_PIN

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 3 - Gas Alert Threshold");

    Wire.begin();

    while (mySensor.begin() == false)
    {
        Serial.println("Gas sensor not connected, check your wiring!");
        delay(1000);
    }

    Serial.print("Connected to a ");
    Serial.print(mySensor.getSensorName());
    Serial.print(" (");
    Serial.print(mySensor.getTargetGas());
    Serial.println(")");

    Serial.print("This part's resistance ");
    Serial.print(mySensor.rsRisesWithGas() ? "RISES" : "FALLS");
    Serial.println(" with gas, so the library will use the");
    Serial.println(mySensor.rsRisesWithGas() ? "  ADC's LOW limit register."
                                             : "  ADC's HIGH limit register.");
    Serial.println();

    // A threshold in response units needs a clean-air reference first.
    Serial.println("Calibrating in clean air...");
    float r0 = 0.0;
    if (mySensor.measureCleanAirResistance(r0) != ksfTkErrOk)
    {
        Serial.println("Clean-air calibration failed! Is the sensor preheated?");
        while (1)
            ;
    }
    Serial.print("R0 (ohm): ");
    Serial.println(r0, 0);

    // Set the trip point, then report where it landed in raw counts so you can see the mapping.
    if (mySensor.setGasAlertResponse(kAlertResponse) != ksfTkErrOk)
    {
        Serial.println("Failed to set the alert threshold!");
        while (1)
            ;
    }

    uint16_t counts = 0;
    mySensor.responseToCounts(kAlertResponse, counts);
    Serial.print("Alerting above a response of ");
    Serial.print(kAlertResponse, 1);
    Serial.print(" (");
    Serial.print(counts);
    Serial.print(" ADC counts), hysteresis ");
    Serial.print(kHysteresis_mV, 0);
    Serial.println(" mV");

    // A little hysteresis stops the alert chattering right at the threshold.
    mySensor.setAlertHysteresis_mV(kHysteresis_mV);

    // Hold the alert until we clear it, so a brief gas event cannot be missed between polls.
    mySensor.enableAlertHold(true);
    mySensor.clearAlertStatus();

#ifdef USE_ALERT_PIN
    // The board fits no pull-up on ALERT, so provide one before enabling the pin.
    pinMode(kAlertPin, kUseInternalPullup ? INPUT_PULLUP : INPUT);

    mySensor.setAlertPinActiveHigh(false); // active low: the pin is pulled LOW on an alert
    mySensor.enableAlertPin(true);

    Serial.print("Reading the ALERT pin on GPIO ");
    Serial.print(kAlertPin);
    Serial.println(kUseInternalPullup ? " (internal pull-up enabled)"
                                      : " (expecting an external 10 kOhm pull-up to 3.3V)");
#endif

    // Hand the sensor over to the ADC. The slowest cycle time is plenty for a MOS gas sensor and
    // uses the least power.
    mySensor.setCycleTime(GM_CYCLE_TIME_2048);

    Serial.println();
    Serial.println("The ADC is now monitoring the sensor on its own.");
    Serial.println();
}

void loop()
{
    bool gasDetected = false;

#ifdef USE_ALERT_PIN
    // Configured active low, so a LOW pin means the ADC has flagged an alert. No I2C traffic at
    // all until we want to know why.
    gasDetected = (digitalRead(kAlertPin) == LOW);
#else
    if (mySensor.getGasAlert(gasDetected) != ksfTkErrOk)
    {
        Serial.println("Error reading the alert status!");
        delay(1000);
        return;
    }
#endif

    if (gasDetected)
    {
        // Read the current value for context - the alert itself came from the hardware.
        float response = 0.0;
        mySensor.readMeasurement();

        Serial.print("*** ");
        Serial.print(mySensor.getTargetGas());
        Serial.print(" THRESHOLD EXCEEDED ***");

        if (mySensor.getGasResponse(response) == ksfTkErrOk)
        {
            Serial.print(" response ");
            Serial.print(response, 3);
        }
        Serial.println();

        // Alert hold is on, so the flag stays set until cleared. It re-asserts on the next
        // conversion if the gas is still there.
        mySensor.clearAlertStatus();
    }
    else
    {
        Serial.println("Air is clear.");
    }

    delay(1000);
}
