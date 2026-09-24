/*
  Example 05 - Find Boards

  Bus discovery for the GM-x02B family, and an honest look at its limits.

  The nine board variants ship at nine different factory addresses (0x50 to 0x5A), so scanning
  finds them. But the onboard ADC101C021 has NO ID REGISTER, which means:

    * scanFamily() can tell you which ADDRESSES are responding. That is a measurement.
    * Guessing the SENSOR from the address is an ASSUMPTION. It holds only while the board's ADR
      jumpers are at their factory setting and nothing else occupies a family address - and note
      that 0x50-0x57 is also the classic 24-series EEPROM block, so SparkFun's own Qwiic EEPROM
      at 0x50 will look exactly like a GM-102B here.

  That assumption matters more than it looks, because the assumed part selects the sensitivity
  curve DIRECTION. Mistake a GM-102B for a GM-602B and every gas reading comes out inverted, with
  no error anywhere to tell you.

  So: beginAutoDetect() is handy for a single-board demo, and isIdentityAssumed() reports that the
  identity was a guess. For anything real, declare the part:

      SfeGM602BArdI2C mySensor;  // or SfeGM102BArdI2C, SfeGM802BArdI2C, ...
      mySensor.begin();          // factory address for that part
      mySensor.begin(0x51);      // or wherever you moved the jumpers to

  probeADC() adds a read-only sanity check that the responding device's registers actually look
  like an ADC101C021. It rejects a blank EEPROM outright and a populated one with high probability,
  but a fully zeroed EEPROM would still pass - so it raises confidence, it does not prove identity.
  It never writes, precisely because the device might BE an EEPROM holding customer data.

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

// Every other example uses a per-part class (SfeGM602BArdI2C and friends), which bakes the board
// identity into the type. This sketch is the one exception, and the awkwardness is the point:
// auto-detection genuinely does not know the part yet, so it uses the generic class with a
// placeholder that beginAutoDetect() then overwrites.
SfeGMX02BArdI2C mySensor(GM_602B);

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 5 - Find Boards");

    Wire.begin();

    // Step 1: what addresses are actually responding? Addresses only - no identity claimed.
    uint8_t found[9];
    uint8_t count = SfeGMX02BArdI2C::scanFamily(found, sizeof(found));

    Serial.print("Family addresses responding: ");
    Serial.println(count);

    for (uint8_t i = 0; i < count; i++)
    {
        Serial.print("  0x");
        Serial.print(found[i], HEX);
        Serial.print(" - would be a ");
        Serial.print(sfe_gmx02b_partName(sfe_gmx02b_sensorFromAddress(found[i])));
        Serial.print(" (");
        Serial.print(sfe_gmx02b_targetGas(sfe_gmx02b_sensorFromAddress(found[i])));
        Serial.println(") at factory jumper settings");
    }

    // Step 2: connect to the first one, requiring the ADC register signature to match.
    while (mySensor.beginAutoDetect() == false)
    {
        Serial.println("Nothing on the bus looked like an ADC101C021 - check your wiring!");
        delay(2000);
    }

    Serial.println();
    Serial.print("Using the board at 0x");
    Serial.print(mySensor.getAddress(), HEX);
    Serial.print(", treating it as a ");
    Serial.println(mySensor.getSensorName());

    if (mySensor.isIdentityAssumed())
    {
        Serial.println();
        Serial.println("*** Sensor type ASSUMED from the I2C address, not measured. ***");
        Serial.print("*** If this is not really a ");
        Serial.print(mySensor.getSensorName());
        Serial.println(", gas readings may be inverted. ***");
        Serial.println("*** Declare the part in the constructor to be certain.        ***");
    }

    Serial.println();
    Serial.println("Raw (0-1023)\tVoltage (V)\tRs (ohm)");
}

void loop()
{
    // Reading is identical for every family member - only the target gas differs.
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
