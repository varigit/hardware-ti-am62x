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

#include "ThermalZone.h"

#include <android-base/logging.h>

#include <fstream>
#include <regex>
#include <unordered_map>

namespace android::hardware::thermal::V2_0::implementation {

ThermalZone::ThermalZone(std::string&& iSysFileName) noexcept
    : ThermalDeviceDir(std::move(iSysFileName)) {
    std::string typeName;

    // Unfortunately, cannot use _temp.name directly (hidl_string);
    getInputStream("type") >> typeName;

    _temp.name = std::move(typeName);
    _temp.type = mapSysfsToTemperatureType(_temp.name);
    _temp.value = -1;
    _temp.throttlingStatus = ThrottlingSeverity::NONE;
}

// Temperature_1_0 conversion operator. Cannot handle some v2.0 temperature types
ThermalZone::operator Temperature_1_0() const {
    return Temperature_1_0{
        .type = static_cast<TemperatureType_1_0>(_temp.type),
        .name = _temp.name,
        .currentValue = _temp.value,
        .throttlingThreshold =
            _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                ThrottlingSeverity::SEVERE)],
        .shutdownThreshold =
            _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                ThrottlingSeverity::SHUTDOWN)],
        .vrThrottlingThreshold = _vrThrottlingThreshold};
}

// Reads sensor's static data
bool ThermalZone::init() {
    std::unique_ptr<DIR, int (*)(DIR*)> thermalPath{opendir(_sysDirPath.c_str()), closedir};

    if (!thermalPath) {
        LOG(ERROR) << __FUNCTION__ << " - Filesystem error(" << strerror(errno) << ")\n";
        return false;
    }

    errno = 0;
    dirent* tz = nullptr;
    const std::regex tzTripTypePat("trip_point_[0-9]+_type$");
    const std::regex tzTripTempPat("_type$");
    bool ok = true;

    while ((tz = readdir(thermalPath.get())) && !errno) {
        if (errno) {
            LOG(ERROR) << __FUNCTION__ << " - Error while listing trip points of " << _sysDirPath
                       << "(" << strerror(errno) << ")\n";
            ok = false;
            break;
        }
        if (std::regex_search(tz->d_name, tzTripTypePat)) {
            // Found a trip_pointX_type file
            std::string tripPointType;

            getInputStream(tz->d_name) >> tripPointType;
            auto tempFile =
                getInputStream(regex_replace(std::string(tz->d_name), tzTripTempPat, "_temp"));
            float temp;

            tempFile >> temp;
            temp /= 1000;

            /* Here, the association between a trip point type('active', 'passive', ...) and a
               ThrottlingSeverity value is completely arbitrary so far */
            if (_kTripPointPassive == tripPointType) {
                _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                    ThrottlingSeverity::MODERATE)] = temp;
            } else if (_kTripPointActive == tripPointType) {
                _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                    ThrottlingSeverity::SEVERE)] = temp;
            } else if (_kTripPointHot == tripPointType) {
                _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                    ThrottlingSeverity::EMERGENCY)] = temp;
            } else if (_kTripPointCritical == tripPointType) {
                _hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                    ThrottlingSeverity::SHUTDOWN)] = temp;
            } else
                LOG(ERROR) << __FUNCTION__ << " - Unknown trip point type\n";
        }
    }

    return ok;
}

TemperatureType ThermalZone::mapSysfsToTemperatureType(const std::string& sysTypeName) {
    static const std::unordered_map<std::string, TemperatureType> sysTypeNameMap = {
        {"main0-thermal", TemperatureType::CPU},  // The one from our .dtsi
        {"main1-thermal", TemperatureType::POWER_AMPLIFIER}  // DDR is unsupported in types.hal 2.0
    };

    auto foundTemp = sysTypeNameMap.find(sysTypeName);

    return (foundTemp != sysTypeNameMap.end() ? foundTemp->second : TemperatureType::UNKNOWN);
}

// Sets the throttling status based on the current temperature and the throttling thresholds.
void ThermalZone::getThrottlingStatus() {
    constexpr size_t severityCount = decltype(_hotThrottlingThresholds)::size();

    _temp.throttlingStatus = ThrottlingSeverity::NONE;

    for (auto i = 0; i < severityCount; ++i) {
        if (_hotThrottlingThresholds[i] != -1) {
            if (_temp.value >= _hotThrottlingThresholds[i])
                _temp.throttlingStatus = static_cast<ThrottlingSeverity>(i);
            else
                break;
        }
    }
}

}  // namespace android::hardware::thermal::V2_0::implementation
