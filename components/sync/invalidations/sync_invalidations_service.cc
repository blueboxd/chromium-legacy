// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service.h"

#include "components/sync/invalidations/fcm_handler.h"

namespace syncer {

SyncInvalidationsService::SyncInvalidationsService(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  fcm_handler_ = std::make_unique<FCMHandler>(gcm_driver, instance_id_driver,
                                              sender_id, app_id);
  fcm_handler_->StartListening();
}

SyncInvalidationsService::~SyncInvalidationsService() = default;

void SyncInvalidationsService::Shutdown() {
  fcm_handler_.reset();
}

}  // namespace syncer
