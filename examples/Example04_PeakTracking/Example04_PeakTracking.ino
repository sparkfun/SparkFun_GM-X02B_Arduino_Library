/*
  Example 04 - Peak Tracking

  The ADC101C021 keeps two extra registers in hardware: the lowest and the highest conversion it
  has seen since each was last cleared. In automatic conversion mode they update on every
  conversion with no I2C traffic, so a gas spike between your polls still gets recorded.

  Which register holds "most gas" depends on the part. On the eight reducing-gas sensors, more gas
  means lower Rs and a HIGHER output voltage, so peak gas is the highest conversion. On the
  GM-102B (NO2) it is the opposite, and peak gas is the LOWEST conversion. getPeakGasVoltage_V()
  and clearPeakGasCounts() pick the right register for you; getLowestConversion() and
  getHighestConversion() remain available if you want the raw registers.

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
// SfeGM102BArd mySensor;   // GM-102B - nitrogen dioxide (NO2)
// SfeGM202BArd mySensor;   // GM-202B - alcohol, smoke
// SfeGM302BArd mySensor;   // GM-302B - ethanol
// SfeGM402BArd mySensor;   // GM-402B - methane, propane
// SfeGM502BArd mySensor;   // GM-502B - VOCs
// SfeGM512BArd mySensor;   // GM-512B - breath odor
SfeGM602BArd mySensor;      // GM-602B - hydrogen sulfide (H2S)
// SfeGM702BArd mySensor;   // GM-702B - carbon monoxide (CO)
// SfeGM802BArd mySensor;   // GM-802B - ammonia (NH3)

// Report and reset the peak every 10 seconds.
const unsigned long kWindowMillis = 10000;
unsigned long windowStart = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 4 - Peak Tracking");

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

    Serial.print("Peak gas is tracked in the ");
    Serial.print(mySensor.rsRisesWithGas() ? "LOWEST" : "HIGHEST");
    Serial.println("-conversion register on this part.");
    Serial.println();

    // The min/max registers only update while the ADC is converting on its own.
    mySensor.setCycleTime(GM_CYCLE_TIME_2048);

    mySensor.clearPeakGasCounts();
    windowStart = millis();

    Serial.println("Current (V)\tPeak (V, 10 s window)");
}

void loop()
{
    float peak;

    mySensor.readMeasurement();
    mySensor.getPeakGasVoltage_V(peak);

    Serial.print(mySensor.getVoltage_V(), 3);
    Serial.print("\t\t");
    Serial.print(peak, 3);

    if (millis() - windowStart >= kWindowMillis)
    {
        Serial.print("\t<-- window reset");
        mySensor.clearPeakGasCounts();
        windowStart = millis();
    }

    Serial.println();
    delay(1000);
}
