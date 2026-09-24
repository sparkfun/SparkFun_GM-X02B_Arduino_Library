/**
 * @file SparkFun_GMX02B.h
 * @brief Arduino-specific implementation for the SparkFun Qwiic MEMS gas sensor breakouts.
 *
 * @details
 * This file provides the Arduino-specific wrapper for the SparkFun Qwiic gas sensor boards built
 * around the Winsen GM-x02B MEMS sensing elements. SfeGMX02BArd inherits from the
 * platform-independent sfDevGMX02B driver and implements I2C communication using Arduino's Wire
 * library via the SparkFun Toolkit.
 *
 * The sensor type and the I2C address are set separately. The installed sensing element cannot be
 * read back from the hardware (the onboard ADC101C021 has no ID register), so it is declared in the
 * constructor. The I2C address is set by jumpers and can be overridden in begin():
 *
 *     SfeGMX02BArd mySensor(GM_602B);
 *     mySensor.begin();          // this part's factory address, 0x58
 *     mySensor.begin(0x51);      // jumpers moved: same sensor, new address, correct math
 *
 * This matters because the declared part selects the sensitivity curve direction (see
 * sfDevGMX02B). If the part were inferred from the address, moving an ADR jumper could cause a
 * GM-102B to be treated as a GM-602B, inverting its readings.
 *
 * @author SparkFun Electronics
 * @date September 2026
 * @copyright Copyright (c) 2026, SparkFun Electronics Inc. This project is released under the MIT License.
 *
 * SPDX-License-Identifier: MIT
 *
 * @see https://github.com/sparkfun/SparkFun_GM-X02B_Arduino_Library
 */

#pragma once

// clang-format off
#include <SparkFun_Toolkit.h>
#include "sfTk/sfDevGMX02B.h"
#include <Arduino.h>
// clang-format on

/**
 * @class SfeGMX02BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-x02B gas sensor breakouts.
 *
 * @details
 * Provides Arduino-specific I2C communication for the board's ADC101C021. Inherits all
 * measurement, calibration, alert, and per-part behavior from sfDevGMX02B and adds the begin()
 * methods required for Arduino. The class owns an sfTkArdI2C bus object which wraps the Arduino
 * Wire library.
 *
 * @see sfDevGMX02B
 * @see TwoWire
 */
class SfeGMX02BArd : public sfDevGMX02B
{
  public:
    /**
     * @brief Construct a driver for a specific board variant.
     *
     * @param sensor The GM-x02B variant installed on the board. Required; see sfDevGMX02B for
     * why there is no default.
     */
    SfeGMX02BArd(sfe_gmx02b_sensor_t sensor) : sfDevGMX02B(sensor)
    {
    }

    /**
     * @brief Initialize the board at the declared part's factory-default I2C address.
     *
     * @note The sensing element needs preheating before its readings are meaningful: minutes for
     * the output to settle after power-up, and 24 to 168 hours of powered aging after long
     * unpowered storage (see the datasheet for this part). A successful begin() means the board is
     * communicating, not that the sensor is warmed up.
     *
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return true if initialization is successful.
     */
    bool begin(TwoWire &wirePort = Wire)
    {
        return begin(getDefaultAddress(), wirePort);
    }

    /**
     * @brief Initialize the board at an explicit I2C address.
     *
     * @details Use this when a board's ADR1 / ADR0 jumpers have been moved off the factory
     * setting. The declared sensor variant, and therefore the curve direction and load resistance,
     * is unaffected, so a re-addressed board still reads correctly.
     *
     * @param address 7-bit I2C address of the board.
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return true if initialization is successful.
     */
    bool begin(uint8_t address, TwoWire &wirePort = Wire)
    {
        // Initialize the Toolkit I2C bus with the given Wire port and address.
        if (_theI2CBus.init(wirePort, address) != ksfTkErrOk)
            return false;

        // Confirm a device is actually responding at this address before we read from it.
        if (_theI2CBus.ping() != ksfTkErrOk)
            return false;

        // The base class begin() confirms the ADC responds to a register read.
        return sfDevGMX02B::begin(&_theI2CBus) == ksfTkErrOk;
    }

    /**
     * @brief Declare the sensor variant and initialize it, in one call.
     *
     * @details Convenience overload equivalent to setSensorType() followed by begin(), for a sketch
     * that picks its part at runtime rather than at construction.
     *
     * @param sensor The GM-x02B variant installed on the board.
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return true if initialization is successful.
     */
    bool begin(sfe_gmx02b_sensor_t sensor, TwoWire &wirePort = Wire)
    {
        if (!setSensorType(sensor))
            return false;

        return begin(getDefaultAddress(), wirePort);
    }

    /**
     * @brief Get the 7-bit I2C address this object is communicating on.
     * @return The configured I2C address.
     */
    uint8_t getAddress(void)
    {
        return _theI2CBus.address();
    }

  private:
    /** @brief Arduino I2C bus interface instance used for all communication with the board. */
    sfTkArdI2C _theI2CBus;
};

///////////////////////////////////////////////////////////////////////////////
// Per-Part Arduino Classes
///////////////////////////////////////////////////////////////////////////////
//
// One thin class per board, following the same per-part pattern as the INA2XX library. Each passes
// its part to the generic constructor, so the part is set by the class type rather than by an
// argument:
//
//     SfeGM602BArd gasSensor;      // no enum to pass
//     gasSensor.begin();           // 0x58
//     gasSensor.begin(0x51);       // re-jumpered board
//
// Prefer these for a sketch that knows its board at compile time. Use SfeGMX02BArd directly
// when the part is chosen at runtime, or when you want one array holding several boards (see
// Example05).

/**
 * @class SfeGM102BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-102B gas sensor breakout (nitrogen dioxide (NO2)).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_102B.
 * Factory I2C address 0x50.
 *
 * @note Rs RISES with gas on this part, the only one in the family that does.
 * The library handles this.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM102BArd : public SfeGMX02BArd
{
  public:
    SfeGM102BArd() : SfeGMX02BArd(GM_102B)
    {
    }
};

/**
 * @class SfeGM202BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-202B gas sensor breakout (alcohol and smoke).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_202B.
 * Factory I2C address 0x51.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM202BArd : public SfeGMX02BArd
{
  public:
    SfeGM202BArd() : SfeGMX02BArd(GM_202B)
    {
    }
};

/**
 * @class SfeGM302BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-302B gas sensor breakout (ethanol vapor).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_302B.
 * Factory I2C address 0x52.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM302BArd : public SfeGMX02BArd
{
  public:
    SfeGM302BArd() : SfeGMX02BArd(GM_302B)
    {
    }
};

/**
 * @class SfeGM402BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-402B gas sensor breakout (methane and propane).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_402B.
 * Factory I2C address 0x54.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM402BArd : public SfeGMX02BArd
{
  public:
    SfeGM402BArd() : SfeGMX02BArd(GM_402B)
    {
    }
};

/**
 * @class SfeGM502BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-502B gas sensor breakout (VOCs).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_502B.
 * Factory I2C address 0x55.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM502BArd : public SfeGMX02BArd
{
  public:
    SfeGM502BArd() : SfeGMX02BArd(GM_502B)
    {
    }
};

/**
 * @class SfeGM512BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-512B gas sensor breakout (breath odor (H2S, ethanol, acetone)).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_512B.
 * Factory I2C address 0x56.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM512BArd : public SfeGMX02BArd
{
  public:
    SfeGM512BArd() : SfeGMX02BArd(GM_512B)
    {
    }
};

/**
 * @class SfeGM602BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-602B gas sensor breakout (hydrogen sulfide (H2S)).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_602B.
 * Factory I2C address 0x58.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM602BArd : public SfeGMX02BArd
{
  public:
    SfeGM602BArd() : SfeGMX02BArd(GM_602B)
    {
    }
};

/**
 * @class SfeGM702BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-702B gas sensor breakout (carbon monoxide (CO)).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_702B.
 * Factory I2C address 0x59.
 *
 * @note The datasheet specifies a cycled heater for this element, not a steady voltage.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM702BArd : public SfeGMX02BArd
{
  public:
    SfeGM702BArd() : SfeGMX02BArd(GM_702B)
    {
    }
};

/**
 * @class SfeGM802BArd
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-802B gas sensor breakout (ammonia (NH3)).
 *
 * @details Convenience class, equivalent to SfeGMX02BArd constructed with ::GM_802B.
 * Factory I2C address 0x5A.
 *
 * @see SfeGMX02BArd
 * @see sfDevGMX02B
 */
class SfeGM802BArd : public SfeGMX02BArd
{
  public:
    SfeGM802BArd() : SfeGMX02BArd(GM_802B)
    {
    }
};
