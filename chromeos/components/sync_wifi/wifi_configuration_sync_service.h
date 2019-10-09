// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/version_info/channel.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace chromeos {

namespace sync_wifi {

class WifiConfigurationBridge;

// A profile keyed service which instantiates and provides access to an instance
// of WifiConfigurationBridge.
class WifiConfigurationSyncService : public KeyedService {
 public:
  WifiConfigurationSyncService(
      version_info::Channel channel,
      syncer::OnceModelTypeStoreFactory create_store_callback);
  ~WifiConfigurationSyncService() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();

 private:
  std::unique_ptr<WifiConfigurationBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationSyncService);
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_WIFI_CONFIGURATION_SYNC_SERVICE_H_
