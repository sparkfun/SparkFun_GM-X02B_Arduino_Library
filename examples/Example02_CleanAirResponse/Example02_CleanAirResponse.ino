/*
  Example 02 - Clean-Air Response

  Relative gas measurement that works identically on every GM-x02B variant. This sketch measures
  the sensor's clean-air resistance (R0) at startup, then reports the gas response continuously.

  WHY "RESPONSE" AND NOT "RATIO"

  Eight of the nine sensing elements detect reducing gases: their resistance FALLS as gas rises,
  so the datasheet's Rs/R0 ratio drops below 1.0. The GM-102B detects NO2, an oxidizing gas, and
  its resistance RISES - so its Rs/R0 climbs ABOVE 1.0 instead. A sketch that tests
  "ratio < 0.5 means gas" is exactly backwards on that one part.

  getGasResponse() removes the trap: it is R0/Rs on falling-Rs parts and Rs/R0 on rising-Rs parts,
  so it sits near 1.0 in clean air and RISES with target gas on all nine. It is also the direction
  the datasheets quote sensitivity in, so it compares directly against getRatedSensitivity():

      GM-602B   datasheet R0/Rs >= 3 in 50 ppm H2S   ->  response >= 3.0
      GM-102B   datasheet R0/Rs <= 0.5 in 2 ppm NO2  ->  response >= 2.0

  getRatio() is still there if you want the raw physical Rs/R0 to plot against a datasheet curve.

  IMPORTANT: Run the calibration in known-clean air, with the sensor fully warmed up. If the board
  has been unpowered for a long time, preheat it first (see the datasheet for your part). For a
  real project, save the measured R0 (e.g. to EEPROM) and restore it with setCleanAirResistance_Ohm()
  instead of recalibrating on every boot.

  SparkFun Electronics
  Date: 2026
  SparkFun code, firmware, and software is released under the MIT License.
    Please see LICENSE.md for further details.

  Hardware Connections:
  IoT RedBoard --> GM-x02B Breakout
  QWIIC --> QWIIC

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

// Flag for an elevated reading. Tune this for your environment. getRatedSensitivity() is a
// reasonable upper anchor (the response this part is specified to reach at its rated ppm).
const float kDetectResponse = 1.5;

// Give the sensing element time to warm up before calibrating. A few minutes is a practical
// minimum for demo purposes, but longer is better.
const unsigned long kWarmupMillis = 3UL * 60UL * 1000UL;

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 2 - Clean-Air Response");

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

    Serial.print("Rated response: ");
    Serial.print(mySensor.getRatedSensitivity(), 1);
    Serial.print(" at ");
    Serial.print(mySensor.getRatedConcentration_ppm());
    Serial.println(" ppm");

    // Let the heated sensing element stabilize before taking the clean-air reference.
    Serial.println("Warming up (3 minutes)... make sure the sensor is in clean air.");
    unsigned long start = millis();
    while (millis() - start < kWarmupMillis)
    {
        float warmupOhms;
        mySensor.readMeasurement();
        mySensor.getSensorResistance_Ohm(warmupOhms);

        Serial.print("Warmup Rs (ohm): ");
        Serial.println(warmupOhms, 0);
        delay(10000);
    }

    // Average 16 readings in clean air and store the result as R0.
    float r0;
    mySensor.measureCleanAirResistance(r0);

    Serial.print("Clean-air resistance R0 (ohm): ");
    Serial.println(r0, 0);
    Serial.println();
    Serial.println("Rs (ohm)\tRs/R0\t\tResponse (1.0 = clean air)");
}

void loop()
{
    float ohms, ratio, response;

    mySensor.readMeasurement();
    mySensor.getSensorResistance_Ohm(ohms);
    mySensor.getRatio(ratio);
    mySensor.getGasResponse(response);

    Serial.print(ohms, 0);
    Serial.print("\t\t");
    Serial.print(ratio, 3);
    Serial.print("\t\t");
    Serial.print(response, 3);

    //Check the threshold.
    if (response > kDetectResponse)
    {
        Serial.print("\t<-- ");
        Serial.print(mySensor.getTargetGas());
        Serial.print(" detected!");
    }

    Serial.println();

    delay(1000);
}
