/**
 * @file sfDevGMX02B.h
 * @brief Header file for the SparkFun Winsen GM-X02B MEMS Gas Sensor Driver.
 *
 * @details
 * sfDevGMX02B is a comms-agnostic driver for the SparkFun Qwiic MEMS gas sensor breakouts, built
 * on the SparkFun Toolkit. The boards share one design: a Winsen GM-x02B metal-oxide semiconductor
 * (MOS) sensing element in series with a load resistor, read by a Texas Instruments ADC101C021
 * 10-bit I2C ADC. Nine sensor variants are covered, one per target gas, each shipping at a unique
 * default I2C address so multiple boards can share a bus.
 *
 * The ADC101C021 also provides a programmable out-of-range alert function: with automatic
 * conversion mode enabled, the ADC monitors the sensor continuously in hardware and flags (or
 * signals on its ALERT pin) whenever the reading crosses a programmable high or low limit - no
 * polling required. The lowest and highest conversion results are tracked in hardware as well.
 *
 * @note SENSOR DIRECTION. Eight of the nine elements sense reducing gases: their resistance (Rs)
 * FALLS as gas concentration rises. The GM-102B senses NO2, an oxidizing gas, and its resistance
 * RISES with concentration. Because Rs sits on the high side of the divider, that inversion
 * propagates to ADC counts, to which alert limit applies, and to which hardware min/max register
 * holds the peak gas reading. Use getGasResponse(), setGasAlertResponse(), getGasAlert() and
 * getPeakGasCounts() to stay direction-agnostic; the raw register accessors are direction-neutral
 * by definition and leave the interpretation to the caller.
 *
 * @note These are heated MOS elements and must be preheated before readings are meaningful: minutes
 * for the output to settle after power-up, and 24 to 168 hours of powered aging after long unpowered
 * storage (the exact schedule is per-part; see the datasheet). Out of the box these sensors are good
 * for relative measurement and threshold alerts. Absolute ppm requires calibration against a known
 * gas concentration - see setResponseCurve().
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

#include <stddef.h>
#include <stdint.h>

// SparkFun Toolkit core headers
#include <sfTk/sfTkII2C.h>
#include <sfTk/sfToolkit.h>

///////////////////////////////////////////////////////////////////////////////
// GM-X02B Sensor Family
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief The Winsen GM-x02B sensing elements carried by the SparkFun Qwiic gas sensor boards.
 *
 * @details These are ordinals, not I2C addresses - they index the per-part descriptor table. Use
 * sfe_gmx02b_partInfo() or getDefaultAddress() to obtain a part's factory address.
 *
 * @note The onboard ADC101C021 has no product ID register, so the sensing element cannot be read
 * back from the hardware. Which variant is installed is a fact about the board that was purchased;
 * it must be declared by the sketch, or assumed from the I2C address (see isIdentityAssumed()).
 */
typedef enum sfe_gmx02b_sensor_t : uint8_t
{
    GM_102B = 0, ///< GM-102B - nitrogen dioxide (NO2). Rs RISES with gas.
    GM_202B,     ///< GM-202B - alcohol and smoke.
    GM_302B,     ///< GM-302B - ethanol vapor.
    GM_402B,     ///< GM-402B - methane and propane (combustibles).
    GM_502B,     ///< GM-502B - VOCs (ethanol, formaldehyde, toluene).
    GM_512B,     ///< GM-512B - breath odor (H2S, ethanol, acetone).
    GM_602B,     ///< GM-602B - hydrogen sulfide (H2S) and benzene.
    GM_702B,     ///< GM-702B - carbon monoxide (CO) and hydrogen.
    GM_802B,     ///< GM-802B - ammonia (NH3).
    GM_PART_COUNT,    ///< Number of parts in the family. Not a valid sensor.
    GM_UNKNOWN = 0xFF ///< No sensor declared or detected.
} sfe_gmx02b_sensor_t;

///////////////////////////////////////////////////////////////////////////////
// Automatic Conversion Cycle Times
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Cycle time settings for the ADC101C021's automatic conversion mode.
 *
 * @details In automatic conversion mode the ADC samples the sensor on its own at the selected
 * interval (expressed as multiples of the ~1 us conversion time), keeps the conversion result
 * register fresh, tracks the lowest / highest conversions, and evaluates the alert limits after
 * every conversion. GM_CYCLE_DISABLED (the power-on default) returns the ADC to normal mode,
 * where a new conversion only occurs when the conversion result register is read.
 */
typedef enum sfe_gmx02b_cycle_time_t : uint8_t
{
    GM_CYCLE_DISABLED = 0x00,  ///< Automatic conversion mode off (normal mode, default).
    GM_CYCLE_TIME_32 = 0x01,   ///< Convert every Tconvert x 32 (~27 ksps).
    GM_CYCLE_TIME_64 = 0x02,   ///< Convert every Tconvert x 64 (~13.5 ksps).
    GM_CYCLE_TIME_128 = 0x03,  ///< Convert every Tconvert x 128 (~6.7 ksps).
    GM_CYCLE_TIME_256 = 0x04,  ///< Convert every Tconvert x 256 (~3.4 ksps).
    GM_CYCLE_TIME_512 = 0x05,  ///< Convert every Tconvert x 512 (~1.7 ksps).
    GM_CYCLE_TIME_1024 = 0x06, ///< Convert every Tconvert x 1024 (~0.9 ksps).
    GM_CYCLE_TIME_2048 = 0x07  ///< Convert every Tconvert x 2048 (~0.4 ksps).
} sfe_gmx02b_cycle_time_t;

///////////////////////////////////////////////////////////////////////////////
// Per-Part Descriptor
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Everything the driver needs to know about one GM-x02B variant and its board.
 *
 * @details One const instance per family member, indexed by ::sfe_gmx02b_sensor_t. This is the only
 * place in the library that carries part-specific knowledge; adding a variant means adding a row.
 *
 * @note The datasheet rsMin / rsMax figures are PART-TO-PART SPREAD at a single reference gas
 * concentration, not the swing a single unit produces. Treat them as a population tolerance.
 */
struct sfe_gmx02b_part_t
{
    uint8_t defaultAddress;     ///< 7-bit I2C address with factory ADR1 / ADR0 jumpers.
    bool rsRisesWithGas;        ///< true if Rs increases with target gas (GM-102B only).
    float loadResistance;       ///< Load resistor RL fitted on this board variant (Ohm).
    float rsMin;                ///< Datasheet Rs lower bound at the reference concentration (Ohm).
    float rsMax;                ///< Datasheet Rs upper bound at the reference concentration (Ohm).
    float ratedSensitivity;     ///< Datasheet bound on the response ratio at ratedConcentration.
    float ratedConcentration;   ///< Concentration at which ratedSensitivity is specified (ppm).
    float rangeMin;             ///< Low end of the detection range (ppm).
    float rangeMax;             ///< High end of the detection range (ppm).
};

///////////////////////////////////////////////////////////////////////////////
// Family Helpers (usable without an instance)
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Get the descriptor for a family member.
 * @param sensor The sensor variant.
 * @return Pointer to the const descriptor, or nullptr if @p sensor is not a valid variant.
 */
const sfe_gmx02b_part_t *sfe_gmx02b_partInfo(sfe_gmx02b_sensor_t sensor);

/**
 * @brief Get a printable part name.
 * @param sensor The sensor variant.
 * @return A string such as "GM-602B", or "Unknown".
 */
const char *sfe_gmx02b_partName(sfe_gmx02b_sensor_t sensor);

/**
 * @brief Get a printable target gas description.
 * @param sensor The sensor variant.
 * @return A string such as "H2S", or "Unknown".
 */
const char *sfe_gmx02b_targetGas(sfe_gmx02b_sensor_t sensor);

/**
 * @brief Map a factory-default I2C address back to its family member.
 *
 * @note This is an ASSUMPTION, not a measurement - it holds only while a board's ADR jumpers are
 * at their factory setting and no other device occupies a family address.
 *
 * @param address The 7-bit I2C address.
 * @return The matching variant, or ::GM_UNKNOWN.
 */
sfe_gmx02b_sensor_t sfe_gmx02b_sensorFromAddress(uint8_t address);

/**
 * @brief Get the family's nine factory-default addresses, in ascending order.
 * @param addresses Output pointer that receives the address table.
 * @return The number of addresses in the table.
 */
uint8_t sfe_gmx02b_familyAddresses(const uint8_t **addresses);

///////////////////////////////////////////////////////////////////////////////
// Class Declaration
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Platform-independent driver for the SparkFun Qwiic MEMS gas sensor breakouts.
 *
 * @details This class implements register-level access to the onboard ADC101C021 via the SparkFun
 * Toolkit bus interface, plus the divider math that turns conversions into sensor resistance and
 * the per-part knowledge that turns resistance into a direction-normalized gas response. Most
 * methods return a SparkFun Toolkit error code (::ksfTkErrOk on success, a negative value on error).
 *
 * The typical polling flow is: begin(), then readMeasurement() followed by getRaw() / getVoltage_V()
 * / getSensorResistance_Ohm(). For relative gas measurement, take a clean-air reference once with
 * measureCleanAirResistance() and then read getGasResponse(). For hardware-monitored threshold
 * alerts, set a limit with setGasAlertResponse(), enable automatic conversion mode with
 * setCycleTime(), and check getGasAlert() (or wire up the board's ALERT pin).
 */
class sfDevGMX02B
{
  public:
    /**
     * @brief Construct a driver for a specific family member.
     *
     * @details The sensing element cannot be identified from the hardware, so declaring it here is
     * how the driver learns the curve direction and the board's load resistance. There is
     * deliberately no default: a wrong part silently inverts the GM-102B's readings and picks the
     * wrong load resistor, so the library makes you say which board you have.
     *
     * @param sensor The sensor variant installed on the board. ::GM_UNKNOWN and
     * out-of-range values are a programming error; they fall back to the GM-602B's circuit
     * parameters (4.7 kOhm, falling Rs) so that no accessor can dereference a null descriptor.
     */
    sfDevGMX02B(sfe_gmx02b_sensor_t sensor) : _theBus{nullptr}
    {
        // Every getter reads through _part, so it must never be left null.
        if (!setSensorType(sensor))
            setSensorType(GM_602B);
    }

    /**
     * @brief Initialize the device driver with the given bus.
     *
     * @details Adopts the supplied bus and confirms the onboard ADC101C021 is responding.
     *
     * @param theBus Pointer to the initialized bus object. If null, a bus set by a prior call is used.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t begin(sfTkIBus *theBus = nullptr);

    /**
     * @brief Check whether the board's ADC is connected and responding.
     *
     * @details Verifies that the device at the configured address has the register signature of an
     * ADC101C021 (see probeADC()). begin() calls this, so a successful begin() means the board
     * both answered and looked right.
     *
     * @note Unlike most SparkFun boards, this one has no ID register to verify identity against -
     * the ADC101C021 does not have one, and neither does the Winsen sensing element. The register
     * signature is the closest available substitute, and it confirms the ADC, never which gas
     * sensor is wired to it.
     *
     * @return true if the device responds and its register signature matches.
     */
    bool isConnected(void);

    /**
     * @brief Check that the responding device looks like an ADC101C021.
     *
     * @details The ADC101C021 carries its 10-bit values in bits 11:2 of each 16-bit register, so
     * D1:D0 and D15:D12 read back as zero on every value register, and D1 of the 8-bit
     * configuration register is reserved-zero. That is 31 required-zero bits across six registers.
     *
     * This check is deliberately READ-ONLY. Addresses 0x50 to 0x57 are also the classic 24-series
     * EEPROM block (SparkFun's own Qwiic EEPROM defaults to 0x50), and writing a probe pattern to
     * a register that turned out to be an EEPROM word address would corrupt a customer's data.
     *
     * @note This confirms the ADC, never the gas sensor. A fully zeroed EEPROM would also pass, so
     * treat a true result as raised confidence rather than proof of identity.
     *
     * @param assumeUnconfigured If true, additionally require the alert limit registers to hold
     * their power-on values (low = 0, high = 1023). Much stronger, but only valid before the
     * sketch has configured the alert function.
     * @return true if the device's registers are consistent with an ADC101C021.
     */
    bool probeADC(bool assumeUnconfigured = false);

    // ========================= Sensor Identity ============================

    /**
     * @brief Declare which family member is installed.
     *
     * @details Also resets the load resistance to the board default for that variant, discarding
     * any prior setLoadResistance_Ohm() override. Does not touch the stored clean-air resistance, which
     * is no longer meaningful if the part changed - re-run measureCleanAirResistance().
     *
     * @param sensor The sensor variant. ::GM_UNKNOWN and out-of-range values are rejected.
     * @return true if the variant was accepted.
     */
    bool setSensorType(sfe_gmx02b_sensor_t sensor);

    /**
     * @brief Get the declared (or assumed) sensor variant.
     * @return The sensor variant.
     */
    sfe_gmx02b_sensor_t getSensorType(void);

    /**
     * @brief Get the descriptor for the current sensor variant.
     * @return Pointer to the const descriptor. Never null.
     */
    const sfe_gmx02b_part_t *getPartInfo(void);

    /**
     * @brief Get a printable name for the current sensor variant.
     * @return A string such as "GM-602B".
     */
    const char *getSensorName(void);

    /**
     * @brief Get a printable target gas description for the current sensor variant.
     * @return A string such as "H2S".
     */
    const char *getTargetGas(void);

    /**
     * @brief Get the factory-default I2C address for the current sensor variant.
     * @return The 7-bit address.
     */
    uint8_t getDefaultAddress(void);

    /**
     * @brief Report whether Rs rises or falls with target gas concentration.
     *
     * @details True only for the GM-102B (NO2, an oxidizing gas). Callers working directly in ADC
     * counts or with the raw alert registers need this to interpret their results; callers using
     * getGasResponse() and friends do not.
     *
     * @return true if sensor resistance increases with gas concentration.
     */
    bool rsRisesWithGas(void);

    /**
     * @brief Report whether the sensor identity was assumed rather than declared.
     *
     * @details Set when the identity came from an I2C address scan instead of the constructor or
     * setSensorType(). An assumed identity is only as good as the board's jumper settings, and it
     * selects the curve direction - so sketches and diagnostics should surface it.
     *
     * @return true if the identity was inferred from the I2C address.
     */
    bool isIdentityAssumed(void);

    /**
     * @brief Get the low end of the part's detection range.
     * @return Concentration in ppm.
     */
    float getRangeMin_ppm(void);

    /**
     * @brief Get the high end of the part's detection range.
     * @return Concentration in ppm.
     */
    float getRangeMax_ppm(void);

    /**
     * @brief Get the datasheet response ratio bound for this part.
     *
     * @details Expressed in the same direction as getGasResponse(): the response the part is
     * specified to reach at getRatedConcentration_ppm(). Useful as a sanity check on a calibration,
     * or as a starting point for an alert threshold.
     *
     * @return The rated response ratio (dimensionless).
     */
    float getRatedSensitivity(void);

    /**
     * @brief Get the concentration at which getRatedSensitivity() is specified.
     * @return Concentration in ppm.
     */
    float getRatedConcentration_ppm(void);

    // ========================= Measurement ================================

    /**
     * @brief Read one conversion from the ADC and cache the result.
     *
     * @details Reads the conversion result register and stores the 10-bit value and the alert flag
     * bit. In normal mode (automatic conversion disabled), reading this register is what triggers
     * the next conversion. In automatic conversion mode, this returns the most recent conversion.
     *
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t readMeasurement(void);

    // ====================== Cached Measurement Values =====================

    /**
     * @brief Get the raw 10-bit ADC value from the most recent readMeasurement().
     *
     * @note Counts rise with gas on eight of the nine parts and FALL with gas on the GM-102B.
     * See rsRisesWithGas().
     *
     * @return Raw ADC counts (0 to 1023).
     */
    uint16_t getRaw(void);

    /**
     * @brief Get the sensor output voltage from the most recent readMeasurement().
     *
     * @details The voltage across the board's load resistor, computed as
     * counts / 1024 x supply voltage (see setSupplyVoltage_V()).
     *
     * @return Sensor output voltage in volts.
     */
    float getVoltage_V(void);

    /**
     * @brief Get the sensing element resistance (Rs) from the most recent readMeasurement().
     *
     * @details Rs is computed from the load resistor divider: Rs = RL x (1024 - counts) / counts.
     * The supply voltage cancels out of this expression, so only the load resistance (see
     * setLoadResistance_Ohm()) affects the result.
     *
     * @param ohms Output reference that receives the sensor resistance in ohms.
     * @return ::ksfTkErrOk on success, or ::ksfTkErrFail if the cached reading is zero counts
     * (resistance too high to measure).
     */
    sfTkError_t getSensorResistance_Ohm(float &ohms);

    /**
     * @brief Get the raw Rs/R0 ratio from the most recent readMeasurement().
     *
     * @details The unnormalized physical ratio, as plotted on the datasheet sensitivity curves:
     * approximately 1.0 in clean air, falling with gas on eight parts and rising with gas on the
     * GM-102B. For a direction-independent quantity, use getGasResponse().
     *
     * @param ratio Output reference that receives Rs/R0.
     * @return ::ksfTkErrOk on success, or ::ksfTkErrFail if no clean-air resistance has been set
     * or the reading is off scale.
     */
    sfTkError_t getRatio(float &ratio);

    /**
     * @brief Get the direction-normalized gas response from the most recent readMeasurement().
     *
     * @details Defined as R0/Rs for falling-Rs parts and Rs/R0 for rising-Rs parts, so the result
     * is approximately 1.0 in clean air and RISES with target gas on every family member. This is
     * also the direction in which the datasheets quote sensitivity, so it compares directly against
     * getRatedSensitivity():
     *
     * - GM-602B: datasheet R0/Rs >= 3 in 50 ppm H2S  ->  getGasResponse() >= 3.0
     * - GM-102B: datasheet R0/Rs <= 0.5 in 2 ppm NO2 ->  getGasResponse() >= 2.0
     *
     * @param response Output reference that receives the gas response ratio.
     * @return ::ksfTkErrOk on success, or ::ksfTkErrFail if no clean-air resistance has been set
     * or the reading is off scale.
     */
    sfTkError_t getGasResponse(float &response);

    /**
     * @brief Get the ADC alert flag from the most recent readMeasurement().
     *
     * @details Bit D15 of the conversion result register. Only reported when the alert flag is
     * enabled with enableAlertFlag(); reflects an alert condition in the Alert Status register.
     *
     * @return true if an alert condition was flagged, false otherwise.
     */
    bool getAlertTriggered(void);

    // ====================== Board Circuit Parameters ======================

    /**
     * @brief Set the sensing circuit supply voltage used by getVoltage_V() and voltageToCounts().
     *
     * @details The ADC101C021 uses its supply as the conversion reference. On the SparkFun boards
     * both the ADC and the sensing loop run from the 3.3 V Qwiic rail (the default).
     *
     * @param volts Supply / reference voltage in volts.
     */
    void setSupplyVoltage_V(float volts);

    /**
     * @brief Get the configured sensing circuit supply voltage.
     * @return Supply voltage in volts (default 3.3 V).
     */
    float getSupplyVoltage_V(void);

    /**
     * @brief Override the load resistance (RL) used by getSensorResistance_Ohm().
     *
     * @details Defaults to the value in the current part's descriptor, so this is only needed for
     * a hand-modified board. Note that setSensorType() resets it to the descriptor value.
     *
     * @param ohms Load resistance in ohms.
     */
    void setLoadResistance_Ohm(float ohms);

    /**
     * @brief Get the configured load resistance.
     * @return Load resistance in ohms.
     */
    float getLoadResistance_Ohm(void);

    /**
     * @brief Convert a voltage into the equivalent 10-bit ADC counts.
     *
     * @details Useful for expressing alert limits in volts. Uses the configured supply voltage
     * and clamps the result to the valid 0 to 1023 range.
     *
     * @param volts Voltage to convert.
     * @return Equivalent ADC counts (0 to 1023).
     */
    uint16_t voltageToCounts(float volts);

    /**
     * @brief Convert a sensor resistance into the equivalent 10-bit ADC counts.
     *
     * @details The inverse of getSensorResistance_Ohm(): counts = 1024 x RL / (Rs + RL).
     *
     * @param ohms Sensor resistance in ohms.
     * @return Equivalent ADC counts (0 to 1023).
     */
    uint16_t resistanceToCounts(float ohms);

    /**
     * @brief Convert a gas response ratio into the equivalent 10-bit ADC counts.
     *
     * @details Applies the part's curve direction and the stored clean-air resistance, then the
     * divider. Requires a clean-air reference.
     *
     * @param response Gas response ratio, in getGasResponse() units.
     * @param counts Output reference that receives the equivalent raw ADC value.
     * @return ::ksfTkErrOk on success, or ::ksfTkErrFail if no clean-air reference is set or
     * @p response is not positive.
     */
    sfTkError_t responseToCounts(float response, uint16_t &counts);

    // ====================== Clean-Air Calibration =========================

    /**
     * @brief Measure and store the clean-air sensor resistance (R0).
     *
     * @details Averages several ADC readings taken in clean air and stores the resulting sensor
     * resistance as R0, the reference for getRatio() and getGasResponse(). Call this in known-clean
     * air after the sensor has been powered and preheated long enough for its output to stabilize.
     *
     * @param r0 Output reference that receives the measured clean-air resistance in ohms.
     * @param samples Number of readings to average (default 16; 0 is treated as 1).
     * @param sampleDelayMs Delay between readings in milliseconds (default 50).
     * @return ::ksfTkErrOk on success, ::ksfTkErrFail if the readings average to zero counts
     * (resistance too high to measure), or an error code on a communication failure.
     */
    sfTkError_t measureCleanAirResistance(float &r0, uint8_t samples = kCleanAirSamples,
                                          uint16_t sampleDelayMs = kCleanAirSampleDelayMs);

    /**
     * @brief Set the clean-air resistance (R0) directly.
     *
     * @details Use this to restore a previously measured R0 (for example, one saved to EEPROM)
     * instead of recalibrating with measureCleanAirResistance().
     *
     * @param ohms Clean-air resistance in ohms.
     */
    void setCleanAirResistance_Ohm(float ohms);

    /**
     * @brief Get the stored clean-air resistance (R0).
     * @return Clean-air resistance in ohms, or 0 if none has been set.
     */
    float getCleanAirResistance_Ohm(void);

    // ====================== Concentration (opt-in) ========================

    /**
     * @brief Set a power-law curve that converts gas response into a concentration.
     *
     * @details Defines ppm = a x pow(getGasResponse(), b). No coefficients are shipped with this
     * library, and none should be inferred from the datasheets: Winsen publishes no numeric curve
     * data for any GM-x02B part, and the GM-602B manual explicitly describes the sensitivity as
     * "non linear, hysteretic and non-repetitive". Fit @p a and @p b yourself against known gas
     * concentrations for your part, in your enclosure, at your operating temperature and humidity.
     *
     * @param a Multiplier.
     * @param b Exponent.
     */
    void setResponseCurve(float a, float b);

    /**
     * @brief Get the estimated target gas concentration.
     *
     * @details Only meaningful after setResponseCurve() and a clean-air reference.
     *
     * @param ppm Output reference that receives the concentration in ppm.
     * @return ::ksfTkErrOk on success, or ::ksfTkErrFail if no curve has been set with
     * setResponseCurve(), or no clean-air reference exists.
     */
    sfTkError_t getConcentration_ppm(float &ppm);

    /**
     * @brief Check whether a concentration falls inside the part's rated detection range.
     * @param ppm Concentration to test.
     * @return true if rangeMin <= ppm <= rangeMax.
     */
    bool isInRange(float ppm);

    // ========================== Alert Function ============================

    /**
     * @brief Set a hardware gas threshold in gas response units.
     *
     * @details Converts @p response into ADC counts and writes it to whichever limit register
     * corresponds to "more gas" for this part - the high limit on falling-Rs parts, the low limit
     * on the GM-102B. Pair with getGasAlert(), which maps the status flags back the same way.
     *
     * @note Writes one limit register and leaves the other alone. If you previously set the other
     * limit by hand, it stays active.
     *
     * @param response Threshold in getGasResponse() units (for example 2.0 for "twice the
     * clean-air response").
     * @return ::ksfTkErrOk on success, ::ksfTkErrFail if no clean-air reference is set, or an
     * error code on a communication failure.
     */
    sfTkError_t setGasAlertResponse(float response);

    /**
     * @brief Read the hardware alert status as a single "gas detected" flag.
     *
     * @details Maps the ADC's under-range / over-range flags through the part's curve direction.
     *
     * @param gasDetected Output reference set true if the gas-side alert is active.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getGasAlert(bool &gasDetected);

    /**
     * @brief Set the alert low limit (under-range threshold).
     *
     * @details Direction-neutral raw register access. If a conversion falls below this value, an
     * under-range alert is generated. The power-on default is 0 (never triggers). On the GM-102B
     * this is the gas-side limit; on every other part it is the clean-air side.
     *
     * @param counts Threshold in raw ADC counts (0 to 1023). See voltageToCounts().
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setAlertLowLimit(uint16_t counts);

    /**
     * @brief Get the alert low limit.
     * @param counts Output reference that receives the threshold in raw ADC counts.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getAlertLowLimit(uint16_t &counts);

    /**
     * @brief Set the alert high limit (over-range threshold).
     *
     * @details Direction-neutral raw register access. If a conversion rises above this value, an
     * over-range alert is generated. The power-on default is 1023 (never triggers). On eight of the
     * nine parts this is the gas-side limit; on the GM-102B it is the clean-air side.
     *
     * @param counts Threshold in raw ADC counts (0 to 1023). See voltageToCounts().
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setAlertHighLimit(uint16_t counts);

    /**
     * @brief Get the alert high limit.
     * @param counts Output reference that receives the threshold in raw ADC counts.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getAlertHighLimit(uint16_t &counts);

    /**
     * @brief Set the alert hysteresis.
     *
     * @details After an alert, the reading must move back inside the violated limit by more than
     * this value before the alert self-clears (unless alert hold is enabled, in which case alerts
     * only clear via clearAlertStatus()).
     *
     * @param counts Hysteresis in raw ADC counts (0 to 1023).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setAlertHysteresis(uint16_t counts);

    /**
     * @brief Get the alert hysteresis.
     * @param counts Output reference that receives the hysteresis in raw ADC counts.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getAlertHysteresis(uint16_t &counts);

    /**
     * @brief Set the alert hysteresis as a sensor-output voltage band.
     *
     * @details Convenience form of setAlertHysteresis() - the ADC's hysteresis field is in raw
     * counts, which is not a quantity anyone thinks in. Hysteresis is linear in the sensor's
     * output voltage, so millivolts convert exactly.
     *
     * @param millivolts Hysteresis in millivolts of sensor output.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setAlertHysteresis_mV(float millivolts);

    /**
     * @brief Get the alert hysteresis as a sensor-output voltage band.
     * @param millivolts Output reference that receives the hysteresis in millivolts.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getAlertHysteresis_mV(float &millivolts);

    /**
     * @brief Enable or disable the alert flag in the conversion result register.
     *
     * @param enable true to enable the flag, false to disable it (default off).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t enableAlertFlag(bool enable);

    /**
     * @brief Enable or disable the ADC's ALERT output pin.
     *
     * @details The ALERT pin is broken out on the board's PTH header. Per the ADC101C021
     * datasheet it "is an open drain data line that must be pulled to the supply (VA) with an
     * external pull-up resistor" - open-drain in BOTH polarity modes, since setAlertPinActiveHigh()
     * changes only the logic sense, not the output structure.
     *
     * @warning THE SPARKFUN BOARD DOES NOT POPULATE A PULL-UP ON ALERT. Nothing holds the pin
     * high on its own, so it floats and reads as garbage until you provide one.
     *
     * @note An external resistor is usually unnecessary: the pin's high-impedance leakage is 1 uA
     * max, so an MCU's internal pull-up (pinMode(pin, INPUT_PULLUP), typically 20-50 kOhm) holds it
     * within ~50 mV of the rail. Fit a discrete 10 kOhm to 3.3 V when the MCU pin has no internal
     * pull-up, when several boards share one alert line, or when a long trace needs faster edges -
     * that gives 330 uA of sink current, 10 mV of droop and a ~0.2 us edge. Stay between 1 kOhm
     * and 100 kOhm. Pulling low is never the problem - VOL is 0.4 V max at 3 mA.
     *
     * @note You do not need the pin at all to use the alert function. getGasAlert() reads the same
     * condition over I2C. When the pin is disabled (the default), it stays high-impedance.
     *
     * @param enable true to enable the ALERT pin, false to disable it.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t enableAlertPin(bool enable);

    /**
     * @brief Set the active polarity of the ALERT output pin.
     * @param activeHigh true for active high, false for active low (default).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setAlertPinActiveHigh(bool activeHigh);

    /**
     * @brief Enable or disable alert hold.
     *
     * @details When enabled, alerts do not self-clear when the reading moves back within the
     * limits - they persist until cleared with clearAlertStatus().
     *
     * @param enable true to hold alerts, false to allow self-clearing.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t enableAlertHold(bool enable);

    /**
     * @brief Read the raw alert status flags.
     *
     * @details Direction-neutral. See getGasAlert() for the part-aware version.
     *
     * @param underRange Output reference set to true if the under-range alert is active.
     * @param overRange Output reference set to true if the over-range alert is active.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getAlertStatus(bool &underRange, bool &overRange);

    /**
     * @brief Clear both alert status flags.
     *
     * @details Writing a one to each flag clears it. If the reading still violates a limit, the
     * alert re-asserts after the next conversion.
     *
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t clearAlertStatus(void);

    // ====================== Automatic Conversion Mode =====================

    /**
     * @brief Set the automatic conversion cycle time (enables / disables automatic mode).
     *
     * @details With a non-zero cycle time, the ADC converts continuously on its own: the alert
     * limits are evaluated and the lowest / highest conversion registers are updated after every
     * conversion, with no I2C traffic required. Pass ::GM_CYCLE_DISABLED to return to normal mode.
     * Slower cycle times use less power; ::GM_CYCLE_TIME_2048 is plenty for a slow-responding MOS
     * gas sensor.
     *
     * @param cycleTime The conversion interval (see ::sfe_gmx02b_cycle_time_t).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t setCycleTime(sfe_gmx02b_cycle_time_t cycleTime);

    /**
     * @brief Get the automatic conversion cycle time.
     * @param cycleTime Output reference that receives the current setting.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getCycleTime(sfe_gmx02b_cycle_time_t &cycleTime);

    /**
     * @brief Get the peak GAS conversion recorded in automatic conversion mode.
     *
     * @details Reads whichever hardware extremum register corresponds to "most gas" for this part -
     * the highest conversion on falling-Rs parts, the lowest on the GM-102B. Only updated while
     * automatic conversion mode is active.
     *
     * @param counts Output reference that receives the raw ADC value.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getPeakGasCounts(uint16_t &counts);

    /**
     * @brief Reset the peak gas tracking register for this part.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t clearPeakGasCounts(void);

    /**
     * @brief Get the lowest conversion result recorded in automatic conversion mode.
     *
     * @details Direction-neutral raw register access. Only updated while automatic conversion mode
     * is active.
     *
     * @param counts Output reference that receives the lowest recorded raw ADC value.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getLowestConversion(uint16_t &counts);

    /**
     * @brief Get the highest conversion result recorded in automatic conversion mode.
     *
     * @details Direction-neutral raw register access. Only updated while automatic conversion mode
     * is active.
     *
     * @param counts Output reference that receives the highest recorded raw ADC value.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t getHighestConversion(uint16_t &counts);

    /**
     * @brief Reset the lowest conversion register (to full scale).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t clearLowestConversion(void);

    /**
     * @brief Reset the highest conversion register (to zero).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t clearHighestConversion(void);

  protected:
    /**
     * @brief Read an 8-bit register from the ADC.
     * @param reg The register pointer value.
     * @param value Output reference that receives the register contents.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t readRegister8(uint8_t reg, uint8_t &value);

    /**
     * @brief Write an 8-bit register on the ADC.
     * @param reg The register pointer value.
     * @param value The value to write.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t writeRegister8(uint8_t reg, uint8_t value);

    /**
     * @brief Read a 16-bit register from the ADC (most significant byte first).
     * @param reg The register pointer value.
     * @param value Output reference that receives the register contents.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t readRegister16(uint8_t reg, uint16_t &value);

    /**
     * @brief Write a 16-bit register on the ADC (most significant byte first).
     * @param reg The register pointer value.
     * @param value The value to write.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t writeRegister16(uint8_t reg, uint16_t value);

    /**
     * @brief Read a 10-bit value register (value stored in bits 11:2).
     * @param reg The register pointer value.
     * @param counts Output reference that receives the 10-bit value.
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t readValueRegister(uint8_t reg, uint16_t &counts);

    /**
     * @brief Write a 10-bit value register (value stored in bits 11:2).
     * @param reg The register pointer value.
     * @param counts The 10-bit value to write (clamped to 1023).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t writeValueRegister(uint8_t reg, uint16_t counts);

    /**
     * @brief Read-modify-write bits of the 8-bit configuration register.
     * @param mask The bits to modify.
     * @param value The new value of the masked bits (already shifted into position).
     * @return ::ksfTkErrOk on success, or an error code on failure.
     */
    sfTkError_t updateConfiguration(uint8_t mask, uint8_t value);

    sfTkIBus *_theBus; ///< Pointer to the communication bus device.

    // --- Sensor identity ---
    sfe_gmx02b_sensor_t _sensorType = GM_602B; ///< Declared or assumed sensor variant.
    const sfe_gmx02b_part_t *_part = nullptr;         ///< Descriptor for _sensorType. Never null after construction.
    bool _identityAssumed = false;                ///< True if _sensorType came from an address scan.

    // --- Cached values from the most recent readMeasurement() ---
    uint16_t _counts = 0;    ///< Raw 10-bit ADC value.
    bool _alertFlag = false; ///< Alert flag (bit D15) from the conversion result.

    // --- Board circuit parameters ---
    float _supplyVoltage = kSupplyVoltageDefault; ///< ADC reference / sensing loop supply (V).
    float _loadResistance = 0.0f;      ///< Load resistor RL (Ohm). Set from the part descriptor.
    float _cleanAirResistance = 0.0f;  ///< Stored clean-air sensor resistance R0 (Ohm); 0 = not set.

    // --- Optional user-supplied concentration curve: ppm = a * pow(response, b) ---
    float _curveA = 0.0f; ///< Multiplier; 0 = no curve set.
    float _curveB = 0.0f; ///< Exponent.

    ///////////////////////////////////////////////////////////////////////////
    // ADC101C021 Register Pointers
    ///////////////////////////////////////////////////////////////////////////
    static const uint8_t kRegConversionResult = 0x00;  ///< Conversion result (16-bit, read only).
    static const uint8_t kRegAlertStatus = 0x01;       ///< Alert status flags (8-bit).
    static const uint8_t kRegConfiguration = 0x02;     ///< Configuration (8-bit).
    static const uint8_t kRegLowLimit = 0x03;          ///< Alert low limit (16-bit).
    static const uint8_t kRegHighLimit = 0x04;         ///< Alert high limit (16-bit).
    static const uint8_t kRegHysteresis = 0x05;        ///< Alert hysteresis (16-bit).
    static const uint8_t kRegLowestConversion = 0x06;  ///< Lowest conversion seen (16-bit).
    static const uint8_t kRegHighestConversion = 0x07; ///< Highest conversion seen (16-bit).

    ///////////////////////////////////////////////////////////////////////////
    // Register Bit Definitions
    ///////////////////////////////////////////////////////////////////////////
    // 16-bit value registers carry their 10-bit value in bits 11:2.
    static const uint8_t kValueShift = 2;          ///< Bit position of the 10-bit value.
    static const uint16_t kValueMask = 0x03FF;     ///< Mask for the 10-bit value (after shifting).
    static const uint16_t kAlertFlagMask = 0x8000; ///< Alert flag (D15) in the conversion result.

    // Bits that must read as zero on a value register (below and above the 10-bit field).
    static const uint16_t kValueReservedMask = 0xF003; ///< D15:D12 and D1:D0.

    // Alert status register bits.
    static const uint8_t kAlertStatusUnderRange = 0x01; ///< Under-range alert flag.
    static const uint8_t kAlertStatusOverRange = 0x02;  ///< Over-range alert flag.

    // Configuration register bits.
    static const uint8_t kConfigCycleTimeMask = 0xE0;   ///< Cycle time field (D7:5).
    static const uint8_t kConfigCycleTimeShift = 5;     ///< Cycle time field bit position.
    static const uint8_t kConfigAlertHold = 0x10;       ///< Alert hold enable (D4).
    static const uint8_t kConfigAlertFlagEnable = 0x08; ///< Conversion result alert flag enable (D3).
    static const uint8_t kConfigAlertPinEnable = 0x04;  ///< ALERT pin enable (D2).
    static const uint8_t kConfigReserved = 0x02;        ///< Reserved, reads zero (D1).
    static const uint8_t kConfigPolarity = 0x01;        ///< ALERT pin polarity: 1 = active high (D0).

    ///////////////////////////////////////////////////////////////////////////
    // Conversion Constants
    ///////////////////////////////////////////////////////////////////////////
    static const uint16_t kAdcFullScale = 1024; ///< 10-bit ADC full-scale count (1 LSB = VA / 1024).
    static const uint16_t kAdcMaxCounts = 1023; ///< Maximum 10-bit ADC value.
    static constexpr float kAdcFullScaleInv = 1.0f / 1024.0f; ///< Precomputed 1 / full scale.

    ///////////////////////////////////////////////////////////////////////////
    // Board Circuit Defaults
    ///////////////////////////////////////////////////////////////////////////
    static constexpr float kSupplyVoltageDefault = 3.3f; ///< Qwiic 3.3 V rail powers ADC and sensing loop.

    ///////////////////////////////////////////////////////////////////////////
    // Clean-Air Calibration Defaults
    ///////////////////////////////////////////////////////////////////////////
    static const uint8_t kCleanAirSamples = 16;        ///< Default readings averaged for R0.
    static const uint16_t kCleanAirSampleDelayMs = 50; ///< Default delay between R0 readings (ms).
};
