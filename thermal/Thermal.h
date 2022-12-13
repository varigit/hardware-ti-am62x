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

#ifndef __THERMAL_CPP__
#define __THERMAL_CPP__

#include <thread>
#include <unordered_map>

#include "CoolDevice.h"
#include "ThermalZone.h"

namespace android::hardware::thermal::V2_0::implementation {

using ::android::hardware::thermal::V1_0::CpuUsage;

class Thermal : public IThermal {
   public:
    // Methods from ::android::hardware::thermal::V1_0::IThermal follow.
    Return<void> getTemperatures(getTemperatures_cb _hidl_cb) override;
    Return<void> getCpuUsages(getCpuUsages_cb _hidl_cb) override;
    Return<void> getCoolingDevices(getCoolingDevices_cb _hidl_cb) override;

    // Methods from ::android::hardware::thermal::V2_0::IThermal follow.
    Return<void> getCurrentTemperatures(bool filterType, TemperatureType type,
                                        getCurrentTemperatures_cb _hidl_cb) override;
    Return<void> getTemperatureThresholds(bool filterType, TemperatureType type,
                                          getTemperatureThresholds_cb _hidl_cb) override;
    Return<void> registerThermalChangedCallback(
        const sp<IThermalChangedCallback>& callback, bool filterType, TemperatureType type,
        registerThermalChangedCallback_cb _hidl_cb) override;
    Return<void> unregisterThermalChangedCallback(
        const sp<IThermalChangedCallback>& callback,
        unregisterThermalChangedCallback_cb _hidl_cb) override;
    Return<void> getCurrentCoolingDevices(bool filterType, CoolingType type,
                                          getCurrentCoolingDevices_cb _hidl_cb) override;

    // Loads the thermal sensors and cooling devices
    bool loadDevices();

    // Starts the monitoring(listener client callbacks) service
    std::thread run();

   private:
    /* _callback_mutex synchronizes acces to _callbacks by (un)registerThermalChangedCallback()
       and our internal monitoring thread itself */
    std::mutex _callback_mutex;

    using callbackItems = std::pair<sp<IThermalChangedCallback>, TemperatureType>;
    /* Unfortunately, having a hash table which key would be the IThermalChangedCallback like below
       isn't possible. Indeed, for a same client callback, we receive a different address upon
       several calls(Binder impact?). std::unordered_map<sp<IThermalChangedCallback>,
       std::pair<bool, TemperatureType>> _callbacks;
    */
    std::vector<callbackItems> _callbacks;

    // Stores thermal zones V2.0 by type(CPU, BATTERY, ...)
    std::unordered_multimap<TemperatureType, ThermalZone> _thermalZones;
    // Stores cooling devices V2.0 by type(FAN, CPU, ...)
    std::unordered_multimap<CoolingType, CoolDevice> _coolingDevices;

    void monitorFunc();
};

}  // namespace android::hardware::thermal::V2_0::implementation

#endif  // #ifndef __THERMAL_CPP__
