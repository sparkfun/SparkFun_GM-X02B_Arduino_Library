/*
  Example 01 - Basic Readings

  Reads a SparkFun Qwiic MEMS gas sensor breakout once per second and prints the raw ADC value,
  the sensor output voltage, and the sensing element resistance (Rs).

  This library covers all nine Winsen GM-x02B variants.

      GM_102B   NO2                     0x50
      GM_202B   Alcohol / smoke         0x51
      GM_302B   Ethanol                 0x52
      GM_402B   CH4 / propane           0x54
      GM_502B   VOC                     0x55
      GM_512B   Breath odor (H2S)       0x56
      GM_602B   H2S                     0x58
      GM_702B   CO                      0x59
      GM_802B   NH3                     0x5A

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

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 1 - Basic Readings");

    Wire.begin();

    while (mySensor.begin() == false)
    {
        Serial.print("No ");
        Serial.print(mySensor.getSensorName());
        Serial.println(" found - check your wiring!");
        delay(1000);
    }

    Serial.print("Connected to a ");
    Serial.print(mySensor.getSensorName());
    Serial.print(" (");
    Serial.print(mySensor.getTargetGas());
    Serial.print(") at 0x");
    Serial.println(mySensor.getAddress(), HEX);

    Serial.print("Detection range: ");
    Serial.print(mySensor.getRangeMin_ppm());
    Serial.print(" to ");
    Serial.print(mySensor.getRangeMax_ppm());
    Serial.println(" ppm");

    Serial.println();
    Serial.println("NOTE: this is a heated MOS sensor. Give it a few minutes to settle after");
    Serial.println("power-up, and 24-168 hours of powered aging after long unpowered storage.");
    Serial.println();
    Serial.println("Raw (0-1023)\tVoltage (V)\tRs (ohm)");
}

void loop()
{
    float ohms;

    mySensor.readMeasurement();
    mySensor.getSensorResistance_Ohm(ohms);

    Serial.print(mySensor.getRaw());
    Serial.print("\t\t");
    Serial.print(mySensor.getVoltage_V(), 3);
    Serial.print("\t\t");
    Serial.println(ohms, 0);

    delay(1000);
}
