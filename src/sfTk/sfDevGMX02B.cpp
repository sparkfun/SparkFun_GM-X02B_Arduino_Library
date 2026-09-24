/**
 * @file sfDevGMX02B.cpp
 * @brief Implementation file for the SparkFun Winsen GM-X02B MEMS Gas Sensor Driver.
 *
 * @details
 * Implements sfDevGMX02B: register-level access to the onboard ADC101C021, the load-resistor
 * divider math, the nine-row per-part descriptor table, and the direction-normalized gas response
 * that lets one API serve both reducing-gas parts (Rs falls with gas) and the GM-102B (Rs rises).
 *
 * @author SparkFun Electronics
 * @date September 2026
 * @copyright Copyright (c) 2026, SparkFun Electronics Inc. This project is released under the MIT License.
 *
 * SPDX-License-Identifier: MIT
 *
 * @see https://github.com/sparkfun/SparkFun_GM-X02B_Arduino_Library
 */

#include "sfDevGMX02B.h"

#include <math.h>

///////////////////////////////////////////////////////////////////////////////
// Per-Part Descriptor Table
///////////////////////////////////////////////////////////////////////////////
//
// One row per family member, in ::sfe_gmx02b_sensor_t order. This is the only part-specific data in
// the library.
//
// Sources: Winsen GM-x02B manuals (various revisions) for the electrical and range figures; the
// SparkFun Qwiic gas sensor board design for the load resistance.
//
// Two things to know about the datasheet numbers:
//
//   1. rsMin / rsMax are PART-TO-PART SPREAD at a single reference gas concentration, not the
//      swing one unit produces. The GM-102B's 10 k to 1 M is a 100:1 manufacturing tolerance.
//
//   2. Winsen specifies RL as "Adjustable" for every part and recommends no value, so
//      loadResistance describes the SparkFun board, not the sensor.
//
// ADR1 / ADR0 jumper decode for reference (tri-level, ADC101C021):
//
//   ADR1     ADR0     Address   Sensor
//   float    float    0x50      GM-102B
//   float    GND      0x51      GM-202B
//   float    3.3V     0x52      GM-302B
//   GND      float    0x54      GM-402B
//   GND      GND      0x55      GM-502B
//   GND      3.3V     0x56      GM-512B
//   3.3V     float    0x58      GM-602B
//   3.3V     GND      0x59      GM-702B
//   3.3V     3.3V     0x5A      GM-802B
//
static const sfe_gmx02b_part_t kGMParts[GM_PART_COUNT] = {
    // GM-102B - NO2. The family's only oxidizing-gas part: Rs RISES with concentration. The
    // datasheet states R0(air)/Rs(2 ppm NO2) <= 0.5, i.e. Rs >= 2 x R0, hence a rated response of
    // 2.0 in getGasResponse() units. Its Rs decade sits far above the other parts', so this board
    // carries a 68.1 k load resistor rather than the family's 4.7 k - with 4.7 k, a unit at the
    // high end of the spread produces under five ADC counts of usable swing.
    // NOTE: Winsen's product page gives the reference concentration as 5 ppm while manual V2.1
    // gives 2 ppm; the manual value is used here.
    {0x50, true, 68100.0f, 10000.0f, 1000000.0f, 2.0f, 2.0f, 0.1f, 10.0f},

    // GM-202B - alcohol (C2H5OH) and smoke.
    {0x51, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 200.0f, 10.0f, 1000.0f},

    // GM-302B - ethanol vapor.
    // NOTE: the manual specifies sensitivity at 5000 ppm ethanol against a 1 to 500 ppm detection
    // range, which is self-inconsistent. 50 ppm is used here as the likely intent; verify against
    // the datasheet revision shipping with your parts before relying on it.
    {0x52, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 50.0f, 1.0f, 500.0f},

    // GM-402B - methane (CH4) and propane (C3H8). Highest heater power in the family (<= 80 mW).
    {0x54, false, 4700.0f, 1000.0f, 30000.0f, 2.0f, 5000.0f, 1.0f, 10000.0f},

    // GM-502B - VOCs: ethanol, formaldehyde, toluene.
    {0x55, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 50.0f, 1.0f, 500.0f},

    // GM-512B - breath / mouth odor: H2S, ethanol, acetone. Baseline resistance decade is an order
    // higher than the other reducing-gas parts (10 k to 100 k), which puts its worst-case
    // quantization at ~2.3 % of reading on the shared 4.7 k load - well below the sensor's own
    // repeatability, so the board keeps the family value.
    {0x56, false, 4700.0f, 10000.0f, 100000.0f, 2.0f, 5.0f, 0.5f, 50.0f},

    // GM-602B - hydrogen sulfide (H2S) and benzene.
    {0x58, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 50.0f, 0.5f, 50.0f},

    // GM-702B - carbon monoxide (CO), also sensitive to H2.
    // NOTE: this is the only part whose datasheet specifies a CYCLED heater - 2.5 V for 60 s then
    // 0.5 V for 90 s - rather than a steady voltage. If the board drives the heater at a fixed
    // voltage, readings will not follow the datasheet's characterization method.
    {0x59, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 150.0f, 5.0f, 5000.0f},

    // GM-802B - ammonia (NH3).
    {0x5A, false, 4700.0f, 1000.0f, 30000.0f, 3.0f, 50.0f, 1.0f, 300.0f},
};

///////////////////////////////////////////////////////////////////////////////
// Family Helpers
///////////////////////////////////////////////////////////////////////////////

const sfe_gmx02b_part_t *sfe_gmx02b_partInfo(sfe_gmx02b_sensor_t sensor)
{
    if (sensor >= GM_PART_COUNT)
        return nullptr;

    return &kGMParts[sensor];
}

// Names live behind a switch rather than in the descriptor struct on purpose: a table of const
// char* references every literal unconditionally, so the linker must keep all nine strings even in
// a sketch that never prints one. With a switch, unused arms are discarded - which matters on an
// AVR, where string literals are copied into RAM at startup.
const char *sfe_gmx02b_partName(sfe_gmx02b_sensor_t sensor)
{
    switch (sensor)
    {
    case GM_102B:
        return "GM-102B";
    case GM_202B:
        return "GM-202B";
    case GM_302B:
        return "GM-302B";
    case GM_402B:
        return "GM-402B";
    case GM_502B:
        return "GM-502B";
    case GM_512B:
        return "GM-512B";
    case GM_602B:
        return "GM-602B";
    case GM_702B:
        return "GM-702B";
    case GM_802B:
        return "GM-802B";
    default:
        return "Unknown";
    }
}

const char *sfe_gmx02b_targetGas(sfe_gmx02b_sensor_t sensor)
{
    switch (sensor)
    {
    case GM_102B:
        return "NO2";
    case GM_202B:
        return "Alcohol / smoke";
    case GM_302B:
        return "Ethanol";
    case GM_402B:
        return "CH4 / C3H8";
    case GM_502B:
        return "VOC";
    case GM_512B:
        return "Breath odor (H2S)";
    case GM_602B:
        return "H2S";
    case GM_702B:
        return "CO";
    case GM_802B:
        return "NH3";
    default:
        return "Unknown";
    }
}

sfe_gmx02b_sensor_t sfe_gmx02b_sensorFromAddress(uint8_t address)
{
    for (uint8_t i = 0; i < GM_PART_COUNT; i++)
    {
        if (kGMParts[i].defaultAddress == address)
            return (sfe_gmx02b_sensor_t)i;
    }

    return GM_UNKNOWN;
}

uint8_t sfe_gmx02b_familyAddresses(const uint8_t **addresses)
{
    // Ascending order, matching the descriptor table.
    static const uint8_t kAddresses[GM_PART_COUNT] = {0x50, 0x51, 0x52, 0x54, 0x55,
                                                        0x56, 0x58, 0x59, 0x5A};

    if (addresses != nullptr)
        *addresses = kAddresses;

    return GM_PART_COUNT;
}

// ========================= Setup & Identity ===============================

sfTkError_t sfDevGMX02B::begin(sfTkIBus *theBus)
{
    // Adopt the supplied bus if one was provided; otherwise keep any bus set by a prior begin().
    if (theBus != nullptr)
        _theBus = theBus;

    // We need a bus to talk to.
    if (_theBus == nullptr)
        return ksfTkErrBusNotInit;

    // Confirm the ADC is present and responding before continuing.
    return isConnected() ? ksfTkErrOk : ksfTkErrBusNoResponse;
}

bool sfDevGMX02B::isConnected(void)
{
    // There is no ID register on either the ADC or the sensing element, so the register-layout
    // signature is the strongest identity check available. See probeADC().
    return probeADC();
}

bool sfDevGMX02B::probeADC(bool assumeUnconfigured)
{
    // Every 16-bit value register carries its 10 bits in D11:D2, so D15:D12 and D1:D0 must read
    // back as zero. Five registers x 6 bits, plus one reserved config bit, is 31 required-zero
    // bits - enough to reject a 24-series EEPROM holding anything but zeros.
    //
    // Deliberately read-only. A write-and-read-back probe would be a stronger test, but at these
    // addresses the device might be an EEPROM, and the "register pointer" would be a word address:
    // the probe itself would destroy customer data.
    static const uint8_t kValueRegs[] = {kRegLowLimit, kRegHighLimit, kRegHysteresis,
                                         kRegLowestConversion, kRegHighestConversion};

    for (uint8_t i = 0; i < sizeof(kValueRegs); i++)
    {
        uint16_t raw = 0;
        if (readRegister16(kValueRegs[i], raw) != ksfTkErrOk)
            return false;

        if ((raw & kValueReservedMask) != 0)
            return false;
    }

    uint8_t config = 0;
    if (readRegister8(kRegConfiguration, config) != ksfTkErrOk)
        return false;

    if ((config & kConfigReserved) != 0)
        return false;

    // Optional stronger check: the alert limits still hold their power-on values. Only valid
    // before anything has configured the alert function.
    if (assumeUnconfigured)
    {
        uint16_t low = 0, high = 0;
        if (readValueRegister(kRegLowLimit, low) != ksfTkErrOk)
            return false;
        if (readValueRegister(kRegHighLimit, high) != ksfTkErrOk)
            return false;
        if (low != 0 || high != kAdcMaxCounts)
            return false;
    }

    return true;
}

bool sfDevGMX02B::setSensorType(sfe_gmx02b_sensor_t sensor)
{
    const sfe_gmx02b_part_t *part = sfe_gmx02b_partInfo(sensor);
    if (part == nullptr)
        return false; // Includes GM_UNKNOWN - leaves the previous part in place.

    _sensorType = sensor;
    _part = part;

    // Adopt this board variant's load resistor, discarding any setLoadResistance_Ohm() override. A
    // stale R0 from a different part would be meaningless, so drop it too.
    _loadResistance = part->loadResistance;
    _cleanAirResistance = 0.0f;
    _identityAssumed = false;

    return true;
}

sfe_gmx02b_sensor_t sfDevGMX02B::getSensorType(void)
{
    return _sensorType;
}

const sfe_gmx02b_part_t *sfDevGMX02B::getPartInfo(void)
{
    return _part;
}

const char *sfDevGMX02B::getSensorName(void)
{
    return sfe_gmx02b_partName(_sensorType);
}

const char *sfDevGMX02B::getTargetGas(void)
{
    return sfe_gmx02b_targetGas(_sensorType);
}

uint8_t sfDevGMX02B::getDefaultAddress(void)
{
    return _part->defaultAddress;
}

bool sfDevGMX02B::rsRisesWithGas(void)
{
    return _part->rsRisesWithGas;
}

bool sfDevGMX02B::isIdentityAssumed(void)
{
    return _identityAssumed;
}

float sfDevGMX02B::getRangeMin_ppm(void)
{
    return _part->rangeMin;
}

float sfDevGMX02B::getRangeMax_ppm(void)
{
    return _part->rangeMax;
}

float sfDevGMX02B::getRatedSensitivity(void)
{
    return _part->ratedSensitivity;
}

float sfDevGMX02B::getRatedConcentration_ppm(void)
{
    return _part->ratedConcentration;
}

// ============================ Measurement =================================

sfTkError_t sfDevGMX02B::readMeasurement(void)
{
    // In normal mode, reading the conversion result register triggers the next conversion. In
    // automatic conversion mode, this returns the most recent automatic conversion.
    uint16_t result = 0;
    sfTkError_t rc = readRegister16(kRegConversionResult, result);
    if (rc != ksfTkErrOk)
        return rc;

    // The 10-bit value lives in bits 11:2; the alert flag (when enabled) is D15.
    _counts = (result >> kValueShift) & kValueMask;
    _alertFlag = (result & kAlertFlagMask) != 0;

    return ksfTkErrOk;
}

// ====================== Cached Measurement Values =========================

uint16_t sfDevGMX02B::getRaw(void)
{
    return _counts;
}

float sfDevGMX02B::getVoltage_V(void)
{
    // The ADC uses its supply as the reference: 1 LSB = VA / 1024.
    return (float)_counts * kAdcFullScaleInv * _supplyVoltage;
}

sfTkError_t sfDevGMX02B::getSensorResistance_Ohm(float &ohms)
{
    ohms = 0.0f;

    // The sensor and load resistor form a divider: Vout = VA x RL / (Rs + RL), which rearranges
    // to Rs = RL x (VA - Vout) / Vout. In ADC counts the supply cancels out entirely:
    // Rs = RL x (1024 - counts) / counts.
    if (_counts == 0)
        return ksfTkErrFail; // No measurable current through the divider - resistance is off scale.

    ohms = _loadResistance * (float)(kAdcFullScale - _counts) / (float)_counts;
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::getRatio(float &ratio)
{
    ratio = 0.0f;

    if (_cleanAirResistance <= 0.0f)
        return ksfTkErrFail; // No clean-air reference has been set.

    float rs = 0.0f;
    sfTkError_t rc = getSensorResistance_Ohm(rs);
    if (rc != ksfTkErrOk)
        return rc;

    ratio = rs / _cleanAirResistance;
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::getGasResponse(float &response)
{
    response = 0.0f;

    if (_cleanAirResistance <= 0.0f)
        return ksfTkErrFail; // No clean-air reference has been set.

    float rs = 0.0f;
    sfTkError_t rc = getSensorResistance_Ohm(rs);
    if (rc != ksfTkErrOk)
        return rc;

    if (rs <= 0.0f)
        return ksfTkErrFail; // Off scale - no usable response.

    // Normalize so 1.0 is clean air and larger always means more target gas, whichever way this
    // part's sensing element moves. This is also the direction the datasheets quote sensitivity in.
    response = _part->rsRisesWithGas ? (rs / _cleanAirResistance) : (_cleanAirResistance / rs);
    return ksfTkErrOk;
}

bool sfDevGMX02B::getAlertTriggered(void)
{
    return _alertFlag;
}

// ====================== Board Circuit Parameters ==========================

void sfDevGMX02B::setSupplyVoltage_V(float volts)
{
    _supplyVoltage = volts;
}

float sfDevGMX02B::getSupplyVoltage_V(void)
{
    return _supplyVoltage;
}

void sfDevGMX02B::setLoadResistance_Ohm(float ohms)
{
    _loadResistance = ohms;
}

float sfDevGMX02B::getLoadResistance_Ohm(void)
{
    return _loadResistance;
}

uint16_t sfDevGMX02B::voltageToCounts(float volts)
{
    if (volts <= 0.0f || _supplyVoltage <= 0.0f)
        return 0;

    float counts = volts / _supplyVoltage * (float)kAdcFullScale + 0.5f;

    if (counts > (float)kAdcMaxCounts)
        return kAdcMaxCounts;

    return (uint16_t)counts;
}

uint16_t sfDevGMX02B::resistanceToCounts(float ohms)
{
    // Inverse of getSensorResistance_Ohm(): counts = 1024 x RL / (Rs + RL).
    if (ohms < 0.0f || _loadResistance <= 0.0f)
        return 0;

    float counts = (float)kAdcFullScale * _loadResistance / (ohms + _loadResistance) + 0.5f;

    if (counts > (float)kAdcMaxCounts)
        return kAdcMaxCounts;

    return (uint16_t)counts;
}

sfTkError_t sfDevGMX02B::responseToCounts(float response, uint16_t &counts)
{
    counts = 0;

    if (_cleanAirResistance <= 0.0f || response <= 0.0f)
        return ksfTkErrFail;

    // Undo the direction normalization to recover the Rs at that response level.
    float rs = _part->rsRisesWithGas ? (_cleanAirResistance * response)
                                     : (_cleanAirResistance / response);

    counts = resistanceToCounts(rs);
    return ksfTkErrOk;
}

// ====================== Clean-Air Calibration =============================

sfTkError_t sfDevGMX02B::measureCleanAirResistance(float &r0, uint8_t samples, uint16_t sampleDelayMs)
{
    r0 = 0.0f;

    if (samples == 0)
        samples = 1;

    // Average the raw counts (rather than the resistances) so a stray zero-count reading cannot
    // pull the average toward infinity.
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
        if (i > 0)
            sftk_delay_ms(sampleDelayMs);

        sfTkError_t rc = readMeasurement();
        if (rc != ksfTkErrOk)
            return rc;

        sum += _counts;
    }

    uint16_t average = (uint16_t)(sum / samples);
    if (average == 0)
        return ksfTkErrFail; // Output too low to compute a resistance - is the sensor preheated?

    r0 = _loadResistance * (float)(kAdcFullScale - average) / (float)average;
    _cleanAirResistance = r0;

    return ksfTkErrOk;
}

void sfDevGMX02B::setCleanAirResistance_Ohm(float ohms)
{
    _cleanAirResistance = ohms;
}

float sfDevGMX02B::getCleanAirResistance_Ohm(void)
{
    return _cleanAirResistance;
}

// ====================== Concentration (opt-in) ============================

void sfDevGMX02B::setResponseCurve(float a, float b)
{
    _curveA = a;
    _curveB = b;
}

sfTkError_t sfDevGMX02B::getConcentration_ppm(float &ppm)
{
    ppm = 0.0f;

    if (_curveA <= 0.0f)
        return ksfTkErrFail; // No curve has been fitted for this sensor.

    float response = 0.0f;
    sfTkError_t rc = getGasResponse(response);
    if (rc != ksfTkErrOk)
        return rc;

    // Kept out of the measurement path deliberately: pow() drags a sizeable chunk of libm into an
    // AVR build, and a sketch that never asks for ppm should not pay for it.
    ppm = _curveA * powf(response, _curveB);
    return ksfTkErrOk;
}

bool sfDevGMX02B::isInRange(float ppm)
{
    return (ppm >= _part->rangeMin) && (ppm <= _part->rangeMax);
}

// ========================== Alert Function ================================

sfTkError_t sfDevGMX02B::setGasAlertResponse(float response)
{
    uint16_t counts = 0;
    sfTkError_t rc = responseToCounts(response, counts);
    if (rc != ksfTkErrOk)
        return rc;

    // A rising-Rs part falls in ADC counts as gas rises, so its gas-side trip point is the LOW
    // limit. Every other part trips on the HIGH limit.
    return _part->rsRisesWithGas ? setAlertLowLimit(counts) : setAlertHighLimit(counts);
}

sfTkError_t sfDevGMX02B::getGasAlert(bool &gasDetected)
{
    gasDetected = false;

    bool under = false, over = false;
    sfTkError_t rc = getAlertStatus(under, over);
    if (rc != ksfTkErrOk)
        return rc;

    gasDetected = _part->rsRisesWithGas ? under : over;
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::setAlertLowLimit(uint16_t counts)
{
    return writeValueRegister(kRegLowLimit, counts);
}

sfTkError_t sfDevGMX02B::getAlertLowLimit(uint16_t &counts)
{
    return readValueRegister(kRegLowLimit, counts);
}

sfTkError_t sfDevGMX02B::setAlertHighLimit(uint16_t counts)
{
    return writeValueRegister(kRegHighLimit, counts);
}

sfTkError_t sfDevGMX02B::getAlertHighLimit(uint16_t &counts)
{
    return readValueRegister(kRegHighLimit, counts);
}

sfTkError_t sfDevGMX02B::setAlertHysteresis(uint16_t counts)
{
    return writeValueRegister(kRegHysteresis, counts);
}

sfTkError_t sfDevGMX02B::getAlertHysteresis(uint16_t &counts)
{
    return readValueRegister(kRegHysteresis, counts);
}

sfTkError_t sfDevGMX02B::setAlertHysteresis_mV(float millivolts)
{
    // The hysteresis field is in ADC counts, and counts are linear in sensor output voltage.
    return setAlertHysteresis(voltageToCounts(millivolts / 1000.0f));
}

sfTkError_t sfDevGMX02B::getAlertHysteresis_mV(float &millivolts)
{
    millivolts = 0.0f;

    uint16_t counts = 0;
    sfTkError_t rc = getAlertHysteresis(counts);
    if (rc != ksfTkErrOk)
        return rc;

    millivolts = (float)counts * kAdcFullScaleInv * _supplyVoltage * 1000.0f;
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::enableAlertFlag(bool enable)
{
    return updateConfiguration(kConfigAlertFlagEnable, enable ? kConfigAlertFlagEnable : 0);
}

sfTkError_t sfDevGMX02B::enableAlertPin(bool enable)
{
    return updateConfiguration(kConfigAlertPinEnable, enable ? kConfigAlertPinEnable : 0);
}

sfTkError_t sfDevGMX02B::setAlertPinActiveHigh(bool activeHigh)
{
    return updateConfiguration(kConfigPolarity, activeHigh ? kConfigPolarity : 0);
}

sfTkError_t sfDevGMX02B::enableAlertHold(bool enable)
{
    return updateConfiguration(kConfigAlertHold, enable ? kConfigAlertHold : 0);
}

sfTkError_t sfDevGMX02B::getAlertStatus(bool &underRange, bool &overRange)
{
    underRange = false;
    overRange = false;

    uint8_t status = 0;
    sfTkError_t rc = readRegister8(kRegAlertStatus, status);
    if (rc != ksfTkErrOk)
        return rc;

    underRange = (status & kAlertStatusUnderRange) != 0;
    overRange = (status & kAlertStatusOverRange) != 0;

    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::clearAlertStatus(void)
{
    // Writing a one to each flag clears it. If the reading still violates a limit, the alert
    // re-asserts after the next conversion.
    return writeRegister8(kRegAlertStatus, kAlertStatusUnderRange | kAlertStatusOverRange);
}

// ====================== Automatic Conversion Mode =========================

sfTkError_t sfDevGMX02B::setCycleTime(sfe_gmx02b_cycle_time_t cycleTime)
{
    return updateConfiguration(kConfigCycleTimeMask, (uint8_t)((uint8_t)cycleTime << kConfigCycleTimeShift));
}

sfTkError_t sfDevGMX02B::getCycleTime(sfe_gmx02b_cycle_time_t &cycleTime)
{
    uint8_t config = 0;
    sfTkError_t rc = readRegister8(kRegConfiguration, config);
    if (rc != ksfTkErrOk)
        return rc;

    cycleTime = (sfe_gmx02b_cycle_time_t)((config & kConfigCycleTimeMask) >> kConfigCycleTimeShift);
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::getPeakGasCounts(uint16_t &counts)
{
    // Peak gas is the highest conversion on a falling-Rs part and the lowest on a rising-Rs one.
    return _part->rsRisesWithGas ? getLowestConversion(counts) : getHighestConversion(counts);
}

sfTkError_t sfDevGMX02B::clearPeakGasCounts(void)
{
    return _part->rsRisesWithGas ? clearLowestConversion() : clearHighestConversion();
}

sfTkError_t sfDevGMX02B::getLowestConversion(uint16_t &counts)
{
    return readValueRegister(kRegLowestConversion, counts);
}

sfTkError_t sfDevGMX02B::getHighestConversion(uint16_t &counts)
{
    return readValueRegister(kRegHighestConversion, counts);
}

sfTkError_t sfDevGMX02B::clearLowestConversion(void)
{
    // Per the datasheet, reset the lowest conversion register by writing full scale.
    return writeValueRegister(kRegLowestConversion, kAdcMaxCounts);
}

sfTkError_t sfDevGMX02B::clearHighestConversion(void)
{
    // Per the datasheet, reset the highest conversion register by writing zero.
    return writeValueRegister(kRegHighestConversion, 0);
}

// ============================ Protected Helpers ===========================

sfTkError_t sfDevGMX02B::readRegister8(uint8_t reg, uint8_t &value)
{
    if (_theBus == nullptr)
        return ksfTkErrBusNotInit;

    size_t readBytes = 0;
    sfTkError_t rc = _theBus->readRegister(&reg, sizeof(reg), &value, sizeof(value), readBytes);
    if (rc != ksfTkErrOk)
        return rc;

    return (readBytes == sizeof(value)) ? ksfTkErrOk : ksfTkErrBusUnderRead;
}

sfTkError_t sfDevGMX02B::writeRegister8(uint8_t reg, uint8_t value)
{
    if (_theBus == nullptr)
        return ksfTkErrBusNotInit;

    // The register pointer byte is followed by the single data byte.
    uint8_t buffer[2] = {reg, value};
    return _theBus->writeData(buffer, sizeof(buffer));
}

sfTkError_t sfDevGMX02B::readRegister16(uint8_t reg, uint16_t &value)
{
    if (_theBus == nullptr)
        return ksfTkErrBusNotInit;

    // The ADC sends 16-bit registers most significant byte first. Read raw bytes and assemble
    // explicitly so the result does not depend on the bus byte-order setting.
    uint8_t buffer[2] = {0};
    size_t readBytes = 0;
    sfTkError_t rc = _theBus->readRegister(&reg, sizeof(reg), buffer, sizeof(buffer), readBytes);
    if (rc != ksfTkErrOk)
        return rc;

    if (readBytes != sizeof(buffer))
        return ksfTkErrBusUnderRead;

    value = ((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1];
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::writeRegister16(uint8_t reg, uint16_t value)
{
    if (_theBus == nullptr)
        return ksfTkErrBusNotInit;

    // The register pointer byte is followed by the data, most significant byte first.
    uint8_t buffer[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return _theBus->writeData(buffer, sizeof(buffer));
}

sfTkError_t sfDevGMX02B::readValueRegister(uint8_t reg, uint16_t &counts)
{
    uint16_t raw = 0;
    sfTkError_t rc = readRegister16(reg, raw);
    if (rc != ksfTkErrOk)
        return rc;

    counts = (raw >> kValueShift) & kValueMask;
    return ksfTkErrOk;
}

sfTkError_t sfDevGMX02B::writeValueRegister(uint8_t reg, uint16_t counts)
{
    if (counts > kAdcMaxCounts)
        counts = kAdcMaxCounts;

    return writeRegister16(reg, (uint16_t)(counts << kValueShift));
}

sfTkError_t sfDevGMX02B::updateConfiguration(uint8_t mask, uint8_t value)
{
    uint8_t config = 0;
    sfTkError_t rc = readRegister8(kRegConfiguration, config);
    if (rc != ksfTkErrOk)
        return rc;

    config = (uint8_t)((config & ~mask) | (value & mask));
    return writeRegister8(kRegConfiguration, config);
}
