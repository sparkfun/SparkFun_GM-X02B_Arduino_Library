/*
  Example 05 - Multiple Sensors

  Three different gases on one Qwiic bus. The nine board variants ship at nine different addresses
  so they can share a bus.

  The loop is identical for all three sensors even though the GM-102B
  runs BACKWARDS from the other two. NO2 is an oxidizing gas, so the GM-102B's resistance rises
  with concentration while the H2S and CO elements fall. getGasResponse() and getGasAlert()
  handle the difference, so application code doesn't need to special-case a part.

  Add or remove sensors by editing the array. Each object declares its own part, so each one gets
  the right address, the right load resistance (the GM-102B board carries 68.1k rather than the
  family's 4.7k), and the right curve direction.

  Several of the same sensor work the same way, since the library doesn't tie a part number to an
  address. Declare the same part three times and give each object the address you jumpered it to:

      SfeGM602BArd intake;    intake.begin();        // 0x58, factory
      SfeGM602BArd exhaust;   exhaust.begin(0x50);   // re-jumpered
      SfeGM602BArd ambient;   ambient.begin(0x55);   // re-jumpered

  The two tri-state ADR jumpers give 3 x 3 = 9 addresses, and any board can use any of them. The
  nine variants ship on different ones so a one-of-each bus works without changes. Nine boards per
  I2C port is therefore the ceiling; pass a different TwoWire to begin() for more.

  Each sensor needs its own clean-air reference, because R0 is a property of an individual
  sensing element, not of a part number. Winsen's part-to-part spread is around 10:1, so two
  GM-602Bs in the same clean air will have different R0 values.

  SparkFun Electronics
  Date: 2026
  SparkFun code, firmware, and software is released under the MIT License.
    Please see LICENSE.md for further details.

  Hardware Connections:
  IoT RedBoard --> GM-x02B Breakouts, daisy-chained
  QWIIC --> QWIIC --> QWIIC

  Open the Serial Monitor at 115200 baud.

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/
*/

#include <SparkFun_GMX02B.h>

// >>> Edit this list to match the boards on your bus. <<<
//
// This uses the GENERIC class rather than the per-part ones (SfeGM602BArd and friends), because
// one array has to hold one type. Per-part classes are nicer when a sketch names each board
// individually (see the comment above).
SfeGMX02BArd sensors[] = {
    SfeGMX02BArd(GM_602B), // H2S at 0x58
    SfeGMX02BArd(GM_702B), // CO  at 0x59
    SfeGMX02BArd(GM_102B), // NO2 at 0x50, resistance RISES with gas
};

const uint8_t kSensorCount = sizeof(sensors) / sizeof(sensors[0]);
bool online[kSensorCount] = {false};

// Flag anything at 1.5x its clean-air response. Characterize real thresholds per gas.
const float kAlertResponse = 1.5;

void setup()
{
    Serial.begin(115200);
    Serial.println("SparkFun GM-x02B Example 5 - Multiple Sensors");

    Wire.begin();

    for (uint8_t i = 0; i < kSensorCount; i++)
    {
        online[i] = sensors[i].begin();

        Serial.print(sensors[i].getSensorName());
        Serial.print(" (");
        Serial.print(sensors[i].getTargetGas());
        Serial.print(") at 0x");
        Serial.print(sensors[i].getDefaultAddress(), HEX);
        Serial.print(": ");

        if (!online[i])
        {
            Serial.println("not found");
            continue;
        }

        Serial.print("ok, RL ");
        Serial.print(sensors[i].getLoadResistance_Ohm(), 0);
        Serial.print(" ohm, Rs ");
        Serial.println(sensors[i].rsRisesWithGas() ? "rises with gas" : "falls with gas");
    }

    // Each element needs its own R0. Do this once, in clean air, warmed up.
    Serial.println();
    Serial.println("Calibrating, make sure the sensors are in clean air...");

    for (uint8_t i = 0; i < kSensorCount; i++)
    {
        if (!online[i])
            continue;

        float r0;
        sensors[i].measureCleanAirResistance(r0);

        Serial.print("  ");
        Serial.print(sensors[i].getSensorName());
        Serial.print(" R0 = ");
        Serial.print(r0, 0);
        Serial.println(" ohm");
    }

    Serial.println();
}

void loop()
{
    // The same code handles every part, regardless of curve direction.
    for (uint8_t i = 0; i < kSensorCount; i++)
    {
        if (!online[i])
            continue;

        float ohms, response;

        sensors[i].readMeasurement();
        sensors[i].getSensorResistance_Ohm(ohms);
        sensors[i].getGasResponse(response);

        Serial.print(sensors[i].getTargetGas());
        Serial.print("\tRs ");
        Serial.print(ohms, 0);
        Serial.print("\tresponse ");
        Serial.print(response, 2);

        if (response > kAlertResponse)
            Serial.print("\t<-- elevated");

        Serial.println();
    }

    Serial.println("---");
    delay(2000);
}
