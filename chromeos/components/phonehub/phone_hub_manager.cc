// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_hub_manager.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "chromeos/components/phonehub/feature_status_provider_impl.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/notification_access_manager_impl.h"

namespace chromeos {
namespace phonehub {
namespace {
PhoneHubManager* g_instance = nullptr;
}  // namespace

// static
PhoneHubManager* PhoneHubManager::Get() {
  return g_instance;
}

PhoneHubManager::PhoneHubManager(
    PrefService* pref_service,
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : feature_status_provider_(std::make_unique<FeatureStatusProviderImpl>(
          device_sync_client,
          multidevice_setup_client)),
      notification_access_manager_(
          std::make_unique<NotificationAccessManagerImpl>(pref_service)),
      phone_model_(std::make_unique<MutablePhoneModel>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

PhoneHubManager::~PhoneHubManager() = default;

void PhoneHubManager::Shutdown() {
  DCHECK(g_instance);
  g_instance = nullptr;

  phone_model_.reset();
  notification_access_manager_.reset();
  feature_status_provider_.reset();
}

}  // namespace phonehub
}  // namespace chromeos
