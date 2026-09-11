/**
 * @file SparkFun_GMX02B.h
 * @brief Arduino-specific implementation for the SparkFun Qwiic MEMS gas sensor breakouts.
 *
 * @details
 * This file provides the Arduino-specific wrapper for the SparkFun Qwiic gas sensor boards built
 * around the Winsen GM-x02B MEMS sensing elements. SfeGMX02BArdI2C inherits from the
 * platform-independent sfDevGMX02B driver and implements I2C communication using Arduino's Wire
 * library via the SparkFun Toolkit. It is the only class a sketch needs.
 *
 * IDENTITY AND ADDRESS ARE SEPARATE THINGS. Which sensing element is installed cannot be read back
 * from the hardware - the onboard ADC101C021 has no ID register - so it is DECLARED, in the
 * constructor. Which I2C address the board answers at is a jumper setting, so it is DISCOVERABLE
 * and can be overridden independently:
 *
 *     SfeGMX02BArdI2C mySensor(GM_602B);
 *     mySensor.begin();          // this part's factory address, 0x58
 *     mySensor.begin(0x51);      // jumpers moved: same sensor, new address, correct math
 *
 * Keeping them separate matters because the declared part selects the sensitivity curve DIRECTION
 * (see sfDevGMX02B). If identity were inferred from the address, a customer moving an ADR jumper to
 * dodge a bus conflict could silently turn a GM-102B into a "GM-602B" and get inverted readings
 * with no error anywhere. beginAutoDetect() is still available for the single-board case, and
 * isIdentityAssumed() reports when the identity was a guess rather than a declaration.
 *
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
 * @class SfeGMX02BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-x02B gas sensor breakouts.
 *
 * @details
 * Provides Arduino-specific I2C communication for the board's ADC101C021. Inherits all
 * measurement, calibration, alert, and per-part behavior from sfDevGMX02B and adds the begin()
 * methods and bus discovery helpers required for Arduino. The class owns an sfTkArdI2C bus object
 * which wraps the Arduino Wire library.
 *
 * @see sfDevGMX02B
 * @see TwoWire
 */
class SfeGMX02BArdI2C : public sfDevGMX02B
{
  public:
    /**
     * @brief Construct a driver for a specific board variant.
     *
     * @param sensor The GM-x02B variant installed on the board. Required - see sfDevGMX02B for
     * why there is no default.
     */
    SfeGMX02BArdI2C(sfe_gmx02b_sensor_t sensor) : sfDevGMX02B(sensor)
    {
    }

    /**
     * @brief Initialize the board at the declared part's factory-default I2C address.
     *
     * @note The sensing element needs preheating before its readings are meaningful: minutes for
     * the output to settle after power-up, and 24 to 168 hours of powered aging after long
     * unpowered storage (see the datasheet for this part). begin() succeeding means the board is
     * communicating - not that the sensor is warmed up.
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
     * setting. The declared sensor variant - and therefore the curve direction and load resistance
     * - is unaffected, which is the point: a re-addressed board still reads correctly.
     *
     * @param address 7-bit I2C address of the board.
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return true if initialization is successful.
     */
    bool begin(uint8_t address, TwoWire &wirePort = Wire)
    {
        // Remember what was asked for, even if the bus init fails - findMisplacedBoard() needs the
        // address this object was pointed at, which is not the part's factory address once a board
        // has been deliberately re-jumpered.
        _requestedAddress = address;

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
     * @brief Scan the family addresses and connect to the first board found, ASSUMING its identity.
     *
     * @details Tries each of the nine factory-default addresses in ascending order (0x50 to 0x5A),
     * connects to the first that responds, and infers the sensor variant from that address. After
     * success, getSensorName() reports the assumed part, getAddress() reports the address, and
     * isIdentityAssumed() returns true.
     *
     * @warning The inferred identity selects the sensitivity curve direction, and it is only as
     * good as the board's jumper settings. Addresses 0x50 to 0x57 are also the classic 24-series
     * EEPROM block - SparkFun's own Qwiic EEPROM defaults to 0x50 - so on a mixed bus this can
     * latch onto the wrong device entirely. Prefer declaring the part in the constructor for
     * anything beyond a single-board demo.
     *
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @param requireAdcProbe If true (the default), only accept an address whose registers look
     * like an ADC101C021. See sfDevGMX02B::probeADC() for what that does and does not prove.
     * @return true if a board was found and initialized.
     */
    bool beginAutoDetect(TwoWire &wirePort = Wire, bool requireAdcProbe = true)
    {
        const uint8_t *addresses = nullptr;
        uint8_t count = sfe_gmx02b_familyAddresses(&addresses);

        for (uint8_t i = 0; i < count; i++)
        {
            if (!begin(addresses[i], wirePort))
                continue;

            if (requireAdcProbe && !probeADC())
                continue;

            sfe_gmx02b_sensor_t sensor = sfe_gmx02b_sensorFromAddress(addresses[i]);
            if (!setSensorType(sensor))
                continue;

            // setSensorType() adopted this part's load resistance and cleared the stale R0, but it
            // also cleared the assumed-identity flag - the identity here really is a guess.
            _identityAssumed = true;
            return true;
        }

        return false;
    }

    /**
     * @brief List which family addresses currently have something responding on the bus.
     *
     * @details A discovery utility: it returns ADDRESSES, not identities, and makes no claim about
     * what is answering. Pair it with begin() plus probeADC() to check a candidate, or use it to
     * report a bus layout to the user.
     *
     * @param found Output array that receives the responding addresses, in ascending order.
     * @param maxFound Capacity of @p found.
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return The number of addresses written to @p found.
     */
    static uint8_t scanFamily(uint8_t *found, uint8_t maxFound, TwoWire &wirePort = Wire)
    {
        if (found == nullptr || maxFound == 0)
            return 0;

        const uint8_t *addresses = nullptr;
        uint8_t count = sfe_gmx02b_familyAddresses(&addresses);
        uint8_t n = 0;

        for (uint8_t i = 0; i < count && n < maxFound; i++)
        {
            sfTkArdI2C bus;

            if (bus.init(wirePort, addresses[i]) != ksfTkErrOk)
                continue;

            if (bus.ping() != ksfTkErrOk)
                continue;

            found[n++] = addresses[i];
        }

        return n;
    }

    /**
     * @brief Diagnose the most likely wiring failure: a board whose address jumpers were moved.
     *
     * @details Call this after begin() has failed. If the address this object was pointed at is
     * silent but exactly ONE other family address is responding, that address is almost certainly
     * the board, re-jumpered - and passing it to begin() will read it correctly. If several
     * addresses answer, the situation is ambiguous and this reports nothing rather than guessing.
     *
     * @note This is a SINGLE-BOARD diagnostic. On a bus carrying several family boards there is no
     * way to tell which one moved, so it returns 0 - a safe no-answer rather than a wrong one.
     *
     * @param wirePort TwoWire instance to use for I2C communication (default: Wire).
     * @return The single responding family address, or 0 if the expected address answers, nothing
     * answers, or more than one other address answers.
     */
    uint8_t findMisplacedBoard(TwoWire &wirePort = Wire)
    {
        // Compare against the address we were actually told to use, falling back to the part's
        // factory address if begin() has not been called yet.
        uint8_t expected = (_requestedAddress != 0) ? _requestedAddress : getDefaultAddress();

        uint8_t found[GM_PART_COUNT] = {0};
        uint8_t n = scanFamily(found, sizeof(found), wirePort);

        uint8_t candidate = 0;
        uint8_t others = 0;

        for (uint8_t i = 0; i < n; i++)
        {
            if (found[i] == expected)
                return 0; // The expected address is answering - this is a different problem.

            candidate = found[i];
            others++;
        }

        return (others == 1) ? candidate : 0;
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
    /** @brief The address most recently passed to begin(); 0 until then. */
    uint8_t _requestedAddress = 0;

    /** @brief Arduino I2C bus interface instance used for all communication with the board. */
    sfTkArdI2C _theI2CBus;
};

///////////////////////////////////////////////////////////////////////////////
// Per-Part Arduino Classes
///////////////////////////////////////////////////////////////////////////////
//
// One thin class per board, following the same pattern as the INA2XX library's SfeINA228ArdI2C /
// SfeINA237ArdI2C. Each simply hands its part to the generic constructor, which means the board
// identity lives in the TYPE rather than in an argument:
//
//     SfeGM602BArdI2C gasSensor;      // no enum to pass, no part to get wrong
//     gasSensor.begin();              // 0x58
//     gasSensor.begin(0x51);          // re-jumpered board, same correct math
//
// Prefer these for a sketch that knows its board at compile time. Use SfeGMX02BArdI2C directly
// when the part is chosen at runtime, or when you want one array holding several boards - see
// Example06.

/**
 * @class SfeGM102BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-102B gas sensor breakout (nitrogen dioxide (NO2)).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_102B, with
 * the part baked into the type. Factory I2C address 0x50.
 *
 * @note Rs RISES with gas on this part - the only one in the family that does. The library handles it.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM102BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM102BArdI2C() : SfeGMX02BArdI2C(GM_102B)
    {
    }
};

/**
 * @class SfeGM202BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-202B gas sensor breakout (alcohol and smoke).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_202B, with
 * the part baked into the type. Factory I2C address 0x51.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM202BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM202BArdI2C() : SfeGMX02BArdI2C(GM_202B)
    {
    }
};

/**
 * @class SfeGM302BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-302B gas sensor breakout (ethanol vapor).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_302B, with
 * the part baked into the type. Factory I2C address 0x52.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM302BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM302BArdI2C() : SfeGMX02BArdI2C(GM_302B)
    {
    }
};

/**
 * @class SfeGM402BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-402B gas sensor breakout (methane and propane).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_402B, with
 * the part baked into the type. Factory I2C address 0x54.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM402BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM402BArdI2C() : SfeGMX02BArdI2C(GM_402B)
    {
    }
};

/**
 * @class SfeGM502BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-502B gas sensor breakout (VOCs).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_502B, with
 * the part baked into the type. Factory I2C address 0x55.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM502BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM502BArdI2C() : SfeGMX02BArdI2C(GM_502B)
    {
    }
};

/**
 * @class SfeGM512BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-512B gas sensor breakout (breath odor (H2S, ethanol, acetone)).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_512B, with
 * the part baked into the type. Factory I2C address 0x56.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM512BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM512BArdI2C() : SfeGMX02BArdI2C(GM_512B)
    {
    }
};

/**
 * @class SfeGM602BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-602B gas sensor breakout (hydrogen sulfide (H2S)).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_602B, with
 * the part baked into the type. Factory I2C address 0x58.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM602BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM602BArdI2C() : SfeGMX02BArdI2C(GM_602B)
    {
    }
};

/**
 * @class SfeGM702BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-702B gas sensor breakout (carbon monoxide (CO)).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_702B, with
 * the part baked into the type. Factory I2C address 0x59.
 *
 * @note The datasheet specifies a cycled heater for this element, not a steady voltage.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM702BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM702BArdI2C() : SfeGMX02BArdI2C(GM_702B)
    {
    }
};

/**
 * @class SfeGM802BArdI2C
 * @brief Arduino I2C implementation for the SparkFun Qwiic GM-802B gas sensor breakout (ammonia (NH3)).
 *
 * @details Convenience class - identical to SfeGMX02BArdI2C constructed with ::GM_802B, with
 * the part baked into the type. Factory I2C address 0x5A.
 *
 * @see SfeGMX02BArdI2C
 * @see sfDevGMX02B
 */
class SfeGM802BArdI2C : public SfeGMX02BArdI2C
{
  public:
    SfeGM802BArdI2C() : SfeGMX02BArdI2C(GM_802B)
    {
    }
};
