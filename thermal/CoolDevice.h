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

#ifndef __COOL_DEVICE_CPP__
#define __COOL_DEVICE_CPP__

#include "ThermalZone.h"

namespace android::hardware::thermal::V2_0::implementation {

using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingType_1_0 = ::android::hardware::thermal::V1_0::CoolingType;

// Ensures that CoolingDeviceType_1_0 is just a subset of CoolingDeviceType_2_0
static_assert(static_cast<std::underlying_type_t<CoolingType_1_0>>(CoolingType_1_0::FAN_RPM) ==
              static_cast<std::underlying_type_t<CoolingType>>(CoolingType::FAN));

// Describes a cooling device
class CoolDevice : public ThermalDeviceDir {
   public:
    CoolDevice(std::string&& iSysFileName) noexcept;

    CoolingDevice _dev;  // Unfortunately, CoolingDevice struct is 'final'

    // CoolingDevice_1_0 conversion operator.Cannot handle some v2.0 cooling types
    operator CoolingDevice_1_0() const;

    // Gets the current zone's temperature
    void readValue() { getInputStream("cur_state") >> _dev.value; }

   private:
    /* Maps a cooling type(CPU, BATTERY, ...) to a given sensor type name
       (contained in /sys/class/cooling_device[0-9]+/type, e.g fan, processor, ... */
    static CoolingType mapSysfsToCoolingType(const std::string& sysTypeName);
};

}  // namespace android::hardware::thermal::V2_0::implementation

#endif  //  #ifndef __COOL_DEVICE_CPP__
