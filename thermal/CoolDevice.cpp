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

#include "CoolDevice.h"

#include <android-base/logging.h>

#include <fstream>
#include <regex>
#include <unordered_map>

namespace android::hardware::thermal::V2_0::implementation {

CoolDevice::CoolDevice(std::string&& iSysFileName) noexcept
    : ThermalDeviceDir(std::move(iSysFileName)) {
    std::string typeName;

    // Unfortunately, cannot use _dev.name directly (hidl_string);
    getInputStream("type") >> typeName;

    _dev.name = std::move(typeName);
    _dev.type = mapSysfsToCoolingType(_dev.name);
    _dev.value = 0;
}

// CoolingDevice_1_0 conversion operator. Cannot handle some v2.0 cooling types
CoolDevice::operator CoolingDevice_1_0() const {
    return CoolingDevice_1_0{.type = static_cast<CoolingType_1_0>(_dev.type),
                             .name = _dev.name,
                             .currentValue = static_cast<float>(_dev.value)};
}

CoolingType CoolDevice::mapSysfsToCoolingType(const std::string& sysTypeName) {
    // Haven't found nothing in dts/* to figure out any mapping
    static const std::unordered_map<std::string, CoolingType> sysTypeNameMap = {
        {"fan", CoolingType::FAN},
        {"Fan", CoolingType::FAN},
        {"processor", CoolingType::CPU},
        {"Processor", CoolingType::CPU}};

    auto foundCooling = sysTypeNameMap.find(sysTypeName);

    // Unfortunately there's no UNKNOWN value for cooling type, this can lead to confusion...
    return (foundCooling != sysTypeNameMap.end() ? foundCooling->second : CoolingType::FAN);
}

}  // namespace android::hardware::thermal::V2_0::implementation
