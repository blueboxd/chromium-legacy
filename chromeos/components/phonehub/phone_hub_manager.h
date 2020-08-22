// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace phonehub {

class FeatureStatusProvider;
class NotificationAccessManager;
class PhoneModel;
class TetherController;

// Implements the core logic of the Phone Hub feature and exposes interfaces via
// its public API. Implemented as a KeyedService which is keyed by the primary
// Profile; since there is only one primary Profile, the class is intended to be
// a singleton.
class PhoneHubManager : public KeyedService {
 public:
  PhoneHubManager(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  PhoneHubManager(const PhoneHubManager&) = delete;
  PhoneHubManager& operator=(const PhoneHubManager&) = delete;
  ~PhoneHubManager() override;

  FeatureStatusProvider* feature_status_provider() {
    return feature_status_provider_.get();
  }

  NotificationAccessManager* notification_access_manager() {
    return notification_access_manager_.get();
  }

  PhoneModel* phone_model() { return phone_model_.get(); }

  TetherController* tether_controller() { return tether_controller_.get(); }

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<FeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<NotificationAccessManager> notification_access_manager_;
  std::unique_ptr<PhoneModel> phone_model_;
  std::unique_ptr<TetherController> tether_controller_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
