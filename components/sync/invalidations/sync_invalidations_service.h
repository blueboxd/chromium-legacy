// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_

#include <memory>
#include <string>

#include "components/keyed_service/core/keyed_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMHandler;

// Service which is used to register with FCM. It is used to obtain an FCM token
// which is used to send invalidations from the server. The service also
// provides incoming invalidations handling and an interface to subscribe to
// invalidations.
class SyncInvalidationsService : public KeyedService {
 public:
  SyncInvalidationsService(gcm::GCMDriver* gcm_driver,
                           instance_id::InstanceIDDriver* instance_id_driver,
                           const std::string& sender_id,
                           const std::string& app_id);
  ~SyncInvalidationsService() override;

  void Shutdown() override;

 private:
  std::unique_ptr<FCMHandler> fcm_handler_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
