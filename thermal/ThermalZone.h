/*
 * Copyright (C) 2018 The Android Open Source Project
 * Copyright (C) 2022 BayLibre SAS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __THERMAL_ZONE_CPP__
#define __THERMAL_ZONE_CPP__

#include <android/hardware/thermal/2.0/IThermal.h>

#include <fstream>

namespace android::hardware::thermal::V2_0::implementation {

using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using TemperatureType_1_0 = ::android::hardware::thermal::V1_0::TemperatureType;

// Ensures that TemperatureType_1_0 is just a subset of TemperatureType_2_0
static_assert(
    static_cast<std::underlying_type_t<TemperatureType_1_0>>(TemperatureType_1_0::UNKNOWN) ==
    static_cast<std::underlying_type_t<TemperatureType>>(TemperatureType::UNKNOWN));
static_assert(static_cast<std::underlying_type_t<TemperatureType_1_0>>(TemperatureType_1_0::CPU) ==
              static_cast<std::underlying_type_t<TemperatureType>>(TemperatureType::CPU));
static_assert(static_cast<std::underlying_type_t<TemperatureType_1_0>>(TemperatureType_1_0::GPU) ==
              static_cast<std::underlying_type_t<TemperatureType>>(TemperatureType::GPU));
static_assert(
    static_cast<std::underlying_type_t<TemperatureType_1_0>>(TemperatureType_1_0::BATTERY) ==
    static_cast<std::underlying_type_t<TemperatureType>>(TemperatureType::BATTERY));
static_assert(static_cast<std::underlying_type_t<TemperatureType_1_0>>(TemperatureType_1_0::SKIN) ==
              static_cast<std::underlying_type_t<TemperatureType>>(TemperatureType::SKIN));

class ThermalDeviceDir {
   public:
    static constexpr char _sysThermalPath[] = "/sys/class/thermal/";

    // Full path of thermalzone[0-9]+/ or cooling_device[0-9]+/ directory
    const std::string _sysDirPath;

   protected:
    ThermalDeviceDir(std::string&& iSysDirName)
        : _sysDirPath(std::string(_sysThermalPath).append(std::move(iSysDirName)).append("/")) {}

    // Gets a input stream from a file inside the sensor directory(i.e _sysFileName)
    std::ifstream getInputStream(std::string&& iFileName) const {
        return std::ifstream(std::string(_sysDirPath).append(std::move(iFileName)));
    }
};

// Describes thermal zones as defined in V 2.0.
class ThermalZone : public ThermalDeviceDir {
   private:
    // trip point levels as defined in trip_point_X_type files
    static constexpr char _kTripPointPassive[] = "passive";
    static constexpr char _kTripPointActive[] = "active";
    static constexpr char _kTripPointHot[] = "hot";
    static constexpr char _kTripPointCritical[] = "critical";

    /* Maps a temperature type(CPU, BATTERY, ...) to a given sensor type name
       (contained in /sys/class/thermalzone[0-9]+/type, e.g cpu-thermal, ... */
    static TemperatureType mapSysfsToTemperatureType(const std::string& sysTypeName);

   public:
    ThermalZone(std::string&& iSysFileName) noexcept;

    Temperature _temp;  // Unfortunately, Temperature struct is 'final'
    /* The current thermal zone static data, read from the system file trip points.
       These are temperature values for each level of severity(NONE, ..., SEVERE, ... SHUTDOWN),
       cf ThrottlingSeverity */
    hidl_array<float, 7 /* ThrottlingSeverity#len */> _hotThrottlingThresholds{
        {-1, -1, -1, -1, -1, -1, -1}};
    // Those values are ignored so far
    hidl_array<float, 7 /* ThrottlingSeverity#len */> _coldThrottlingThresholds{
        {-1, -1, -1, -1, -1, -1, -1}};
    float _vrThrottlingThreshold = -1;

    // Temperature_1_0 conversion operator. Cannot handle some v2.0 temperature types
    operator Temperature_1_0() const;

    // Reads sensor's static data
    bool init();

    // Gets the current zone's temperature
    void readTemp() {
        getInputStream("temp") >> _temp.value;
        _temp.value /= 1000;

        getThrottlingStatus();
    }

   protected:
    // Sets the throttling status based on the current temperature and the throttling thresholds
    void getThrottlingStatus();
};

}  // namespace android::hardware::thermal::V2_0::implementation

#endif  // #ifndef _THERMAL_ZONE_CPP__
