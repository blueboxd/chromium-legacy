// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/phone_hub_manager.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "chromeos/components/phonehub/feature_status_provider_impl.h"

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
    device_sync::DeviceSyncClient* device_sync_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : feature_status_provider_(std::make_unique<FeatureStatusProviderImpl>(
          device_sync_client,
          multidevice_setup_client)) {
  DCHECK(!g_instance);
  g_instance = this;
}

PhoneHubManager::~PhoneHubManager() = default;

void PhoneHubManager::Shutdown() {
  DCHECK(g_instance);
  g_instance = nullptr;

  feature_status_provider_.reset();
}

}  // namespace phonehub
}  // namespace chromeos
