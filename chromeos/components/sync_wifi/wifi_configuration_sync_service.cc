// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_sync_service.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/time/default_clock.h"
#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace chromeos {

namespace sync_wifi {

WifiConfigurationSyncService::WifiConfigurationSyncService(
    version_info::Channel channel,
    syncer::OnceModelTypeStoreFactory create_store_callback) {
  bridge_ = std::make_unique<sync_wifi::WifiConfigurationBridge>(
      /* synced_network_updater= */ nullptr,
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::WIFI_CONFIGURATIONS,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      std::move(create_store_callback));
}

WifiConfigurationSyncService::~WifiConfigurationSyncService() = default;

base::WeakPtr<syncer::ModelTypeControllerDelegate>
WifiConfigurationSyncService::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace sync_wifi

}  // namespace chromeos
