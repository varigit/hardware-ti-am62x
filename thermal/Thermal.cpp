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

#include "Thermal.h"

#include <android-base/logging.h>
#include <dirent.h>
#include <hidl/HidlTransportSupport.h>

#include <chrono>
#include <regex>
#include <set>

namespace android::hardware::thermal::V2_0::implementation {

using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    std::vector<Temperature_1_0> temps;
    // Fills dynamic values coming from sys files
    for (auto& tz : _thermalZones) {
        tz.second.readTemp();
        temps.emplace_back(static_cast<Temperature_1_0>(tz.second));
    }

    if (temps.size())
        _hidl_cb({ThermalStatusCode::SUCCESS, {}}, temps);
    else
        _hidl_cb({ThermalStatusCode::FAILURE, "No sensor available"}, temps);

    return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    static constexpr char cpuStats[] = "/proc/stat";
    static constexpr char cpuDirPath[] = "/sys/devices/system/cpu/";
    static const std::regex statPattern{"^(cpu[0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+).*"};

    if (!_hidl_cb) return Void();

    ThermalStatus status{ThermalStatusCode::SUCCESS, {}};

    std::vector<CpuUsage> cpuUsages;
    std::ifstream statData(cpuStats);
    if (!statData.is_open()) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unable to find cpu statistics";
        LOG(ERROR) << status.debugMessage;
    } else {
        std::string line;
        std::match_results<std::string::const_iterator> statItems;

        while (std::getline(statData, line)) {
            std::cout << "Found " << line << " in /proc/stat\n";
            if (std::regex_match(line, statItems, statPattern)) {
                std::cout << "Matching the pattern\n";
                if (statItems.size() == 6) {  // Should always be true
                    std::string cpuName = statItems[1];
                    uint64_t active = std::stoull(statItems[2]) + std::stoull(statItems[3]) +
                                      std::stoull(statItems[4]);
                    uint64_t idle = std::stoull(statItems[5]);
                    // Checks the cpu online status
                    bool isOnline = false;
                    std::ifstream onlineFile{
                        std::string(cpuDirPath).append(cpuName).append("/online")};

                    if (!onlineFile.is_open()) {
                        if (cpuName == "cpu0")
                            isOnline = true;
                        else {
                            status.code = ThermalStatusCode::FAILURE;
                            status.debugMessage = std::string("Unable to find ")
                                                      .append(cpuName)
                                                      .append(" online file");
                            LOG(ERROR) << status.debugMessage;
                        }
                    } else
                        onlineFile >> isOnline;

                    cpuUsages.push_back({cpuName, active, active + idle, isOnline});
                } else
                    std::cout << "But not the items number " << statItems.size() << "\n";
            }
        }
    }

    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    std::vector<CoolingDevice_1_0> devs;
    // Fills dynamic values coming from sys files
    for (auto& [coolType, dev] : _coolingDevices) {
        dev.readValue();
        devs.push_back(static_cast<CoolingDevice_1_0>(dev));
    }

    if (devs.size())
        _hidl_cb({ThermalStatusCode::SUCCESS, {}}, devs);
    else
        _hidl_cb({ThermalStatusCode::FAILURE, "No cooling device"}, devs);

    return Void();
}

// Methods from ::android::hardware::thermal::V2_0::IThermal follow.
Return<void> Thermal::getCurrentTemperatures(bool filterType, TemperatureType type,
                                             getCurrentTemperatures_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    std::vector<Temperature> temps;
    // Fills dynamic values coming from sys files
    for (auto& [tempType, tz] : _thermalZones) {
        if (filterType && type != tempType)
            continue;
        tz.readTemp();
        temps.push_back(tz._temp);
    }

    if (temps.size())
        _hidl_cb({ThermalStatusCode::SUCCESS, {}}, temps);
    else
        _hidl_cb({ThermalStatusCode::FAILURE, "No sensor available"}, temps);

    return Void();
}

Return<void> Thermal::getTemperatureThresholds(bool filterType, TemperatureType type,
                                               getTemperatureThresholds_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    std::vector<TemperatureThreshold> tempThresholds;
    // Fills dynamic values coming from sys files
    for (auto& [tempType, tz] : _thermalZones) {
        if (filterType && type != tempType)
            continue;
        // We assume that if the hotest threshold isn't defined, none is defined and we ignore this
        // sensor.
        if (-1 ==
            tz._hotThrottlingThresholds[static_cast<std::underlying_type_t<ThrottlingSeverity>>(
                ThrottlingSeverity::SHUTDOWN)])
            continue;
        tempThresholds.push_back({.type = tz._temp.type,
                                  .name = tz._temp.name,
                                  .hotThrottlingThresholds = tz._hotThrottlingThresholds,
                                  .coldThrottlingThresholds = tz._coldThrottlingThresholds,
                                  .vrThrottlingThreshold = tz._vrThrottlingThreshold});
    }

    if (tempThresholds.size())
        _hidl_cb({ThermalStatusCode::SUCCESS, {}}, tempThresholds);
    else
        _hidl_cb({ThermalStatusCode::FAILURE, "No temperature threshold"}, tempThresholds);

    return Void();
}

Return<void> Thermal::getCurrentCoolingDevices(bool filterType, CoolingType type,
                                               getCurrentCoolingDevices_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    std::vector<CoolingDevice> devs;
    // Fills dynamic values coming from sys files
    for (auto& [coolType, dev] : _coolingDevices) {
        if (filterType && type != coolType)
            continue;
        dev.readValue();
        devs.push_back(dev._dev);
    }

    if (devs.size())
        _hidl_cb({ThermalStatusCode::SUCCESS, {}}, devs);
    else
        _hidl_cb({ThermalStatusCode::FAILURE, "No cooling device"}, devs);

    return Void();
}

Return<void> Thermal::registerThermalChangedCallback(const sp<IThermalChangedCallback>& callback,
                                                     bool filterType, TemperatureType type,
                                                     registerThermalChangedCallback_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    if (nullptr == callback) {
        LOG(ERROR) << __FUNCTION__ << " null callback";
        _hidl_cb({ThermalStatusCode::FAILURE, "null callback"});
        return Void();
    }

    ThermalStatus status{ThermalStatusCode::SUCCESS, {}};
    {
        std::lock_guard<std::mutex> _lock(_callback_mutex);

        if (std::any_of(_callbacks.begin(), _callbacks.end(), [&callback](const callbackItems& c) {
                                                               return interfacesEqual(c.first, callback); }
            )) {
            status.code = ThermalStatusCode::FAILURE;
            status.debugMessage = "Same callback registered already";
            LOG(ERROR) << status.debugMessage;
        } else {
            if (filterType)
                _callbacks.push_back(std::make_pair(callback, type));
            else
                _callbacks.push_back(std::make_pair(callback, TemperatureType::UNKNOWN));

            LOG(INFO) << "a callback has been registered to ThermalHAL, isFilter: " << filterType
                      << " Type: " << android::hardware::thermal::V2_0::toString(type);
        }
    }

    _hidl_cb(status);
    return Void();
}

Return<void> Thermal::unregisterThermalChangedCallback(
    const sp<IThermalChangedCallback>& callback, unregisterThermalChangedCallback_cb _hidl_cb) {
    if (!_hidl_cb) return Void();

    if (nullptr == callback) {
        LOG(ERROR) << __FUNCTION__ << "null callback";
        _hidl_cb({ThermalStatusCode::FAILURE, "null callback"});
        return Void();
    }

    ThermalStatus status{ThermalStatusCode::SUCCESS, {}};
    bool removed = false;
    // function used in below std::remove_if call
    auto shouldRemove = [&removed, &callback](const callbackItems& c) {
        if (interfacesEqual(c.first, callback)) {
            LOG(INFO) << "a callback has been unregistered to ThermalHAL, filter type: "
                      << android::hardware::thermal::V2_0::toString(c.second);
            removed = true;
            return true;
        }
        return false;
    };

    {
        std::lock_guard<std::mutex> _lock(_callback_mutex);

        _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), shouldRemove),
                         _callbacks.end());
        if (!removed) {
            status.code = ThermalStatusCode::FAILURE;
            status.debugMessage = "The callback was not registered before";
            LOG(ERROR) << status.debugMessage;
        }
    }
    _hidl_cb(status);
    return Void();
}

// Loads the thermal sensors and cooling devices
bool Thermal::loadDevices() {
    _thermalZones.clear();
    _thermalZones.reserve(5);  // Arbitrary value of 5
    _coolingDevices.clear();
    _coolingDevices.reserve(5);  // Arbitrary value of 5

    // Unfortunately, std::filesystem(libc++fs) isn't yet accessible from vendor components
    std::unique_ptr<DIR, int (*)(DIR*)> sysThermalDir{opendir(ThermalZone::_sysThermalPath),
                                                      closedir};

    if (!sysThermalDir) {
        LOG(ERROR) << __FUNCTION__ << " - Filesystem error(" << strerror(errno) << ") regarding "
                   << ThermalZone::_sysThermalPath << "\n";
        return false;
    }

    dirent* thermalFile = nullptr;
    const std::regex tzNamePattern("thermal_zone[0-9]+$");
    const std::regex coolingDevNamePattern("cooling_device[0-9]+$");

    bool ok = true;

    errno = 0;
    while ((thermalFile = readdir(sysThermalDir.get())) && !errno) {
        if (errno) {
            LOG(ERROR) << __FUNCTION__ << " - Error while listing thermal directory ("
                       << strerror(errno) << ")\n";
            ok = false;
            break;
        }
        if (std::regex_search(thermalFile->d_name, tzNamePattern)) {
            // Matches a thermal zone file name
            ThermalZone tz{std::string(thermalFile->d_name)};

            if (tz._temp.type != TemperatureType::UNKNOWN) {
                auto thermalZone = _thermalZones.emplace(tz._temp.type, std::move(tz));
                // Intializes sensor thresholds
                if (!thermalZone->second.init())
                    LOG(ERROR) << __FUNCTION__
                               << " - Error while initializing the sensor threshold ("
                               << strerror(errno) << ")\n";
            } else
                LOG(WARNING) << __FUNCTION__ << " - Ignoring sensor " << tz._temp.name << ")\n";
        } else if (std::regex_search(thermalFile->d_name, coolingDevNamePattern)) {
            // Matches a cooling device file name
            CoolDevice coolingDev{std::string(thermalFile->d_name)};

            _coolingDevices.emplace(coolingDev._dev.type, std::move(coolingDev));
        }
    }

    return ok;
}

/* The thermal monitoring thread function which calls any registered listener.
 * Note that we send any sensor's temperature of interest to a given client, whether the sensor
 * throttling status has changed since its last notification(5 seconds ago) or not.
 */
void Thermal::monitorFunc() {
    using namespace std::chrono_literals;
    constexpr std::chrono::duration sleepDuration = 5s;

    /* We store already read temperatures upon each iteration in case several callbacks relate to
       a given temperature type. May be overkilling against reading each sensor once, let's see */
    std::unordered_multimap<TemperatureType, Temperature> cachedTemps;

    do {
        {
            std::lock_guard<std::mutex> _lock(_callback_mutex);

            if (!_callbacks.empty()) {
                bool allCached = false;
                cachedTemps.clear();

                for (const auto& c : _callbacks) {
                    /* Note that structured binding of _callbacks items isn't possible here
                       due to some use in the below lambda */
                    bool typeFilter = (TemperatureType::UNKNOWN != c.second);
                    auto temps = (typeFilter ? cachedTemps.find(c.second) : cachedTemps.begin());

                    if (temps != cachedTemps.end() && (typeFilter || allCached)) {
                        // We found it in the cache and we aren't in the case where we need to feed
                        // the cache, so we use it
                        for (; temps != cachedTemps.end(); ++temps) {
                            LOG(INFO) << "HalThermal - Cached Temperature " << temps->second.name
                                      << "(" << temps->second.value << "°C)\n";
                            c.first->notifyThrottling(temps->second);
                        }
                    } else {
                        // Here, we read measures from the files, and store it in our cache
                        auto onRead = [&c, &cachedTemps](const ThermalStatus& status,
                                                         hidl_vec<Temperature> temps) {
                            if (ThermalStatusCode::SUCCESS == status.code) {
                                for (const auto& temp : temps) {
                                    c.first->notifyThrottling(temp);
                                    cachedTemps.insert(std::make_pair(temp.type, std::move(temp)));
                                }
                            }
                        };
                        getCurrentTemperatures(typeFilter, c.second, onRead);
                        if (!typeFilter) allCached = true;
                    }
                }
            }
        }
    } while(std::this_thread::sleep_for(sleepDuration), true);
}

std::thread Thermal::run() {
    return std::thread(&Thermal::monitorFunc, this);
}

}  // namespace android::hardware::thermal::V2_0::implementation
