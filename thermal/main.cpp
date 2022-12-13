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

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include <memory>

#include "Thermal.h"

using ::android::OK;
using ::android::status_t;

// libhwbinder:
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;

// Generated HIDL files:
using ::android::hardware::thermal::V2_0::IThermal;
using ::android::hardware::thermal::V2_0::implementation::Thermal;

static int shutdown() {
    LOG(ERROR) << "Thermal Service is shutting down.";
    return 1;
}

int main(int /* argc */, char** /* argv */) {
    LOG(INFO) << "TI Thermal HAL Service2.0 starting...";

    std::shared_ptr<Thermal> service = std::make_shared<Thermal>();

    configureRpcThreadpool(2, true /* callerWillJoin */);

    service->loadDevices();

    status_t status = service->registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service for ThermalHAL (" << status << ")";
        return shutdown();
    }

    LOG(INFO) << "Thermal Service started successfully.";

    auto clientThread = service->run();

    joinRpcThreadpool();
    // We should not get past the joinRpcThreadpool().
    return shutdown();
}
