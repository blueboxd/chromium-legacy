// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_model_type_controller.h"

#include <utility>

#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

PasswordModelTypeController::PasswordModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_in_memory,
    syncer::SyncService* sync_service,
    const base::RepeatingClosure& state_changed_callback)
    : ModelTypeController(syncer::PASSWORDS,
                          std::move(delegate_on_disk),
                          std::move(delegate_in_memory)),
      sync_service_(sync_service),
      state_changed_callback_(state_changed_callback) {}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->AddObserver(this);
  storage_option_ = configure_context.storage_option;
  ModelTypeController::LoadModels(configure_context, model_load_callback);
  state_changed_callback_.Run();
}

void PasswordModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->RemoveObserver(this);
  // The account storage should be cleared if Sync is stopped for any reason
  // (other than just browser shutdown). One common case it switching from
  // Sync-the-transport to Sync-the-feature; in that case we don't want to end
  // up with two copied of the passwords (one in the profile DB, one in the
  // account DB).
  if (storage_option_ == syncer::STORAGE_IN_MEMORY) {
    switch (shutdown_reason) {
      case syncer::STOP_SYNC:
        shutdown_reason = syncer::DISABLE_SYNC;
        break;
      case syncer::DISABLE_SYNC:
      case syncer::BROWSER_SHUTDOWN:
        break;
    }
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
  state_changed_callback_.Run();
}

void PasswordModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  state_changed_callback_.Run();
}

}  // namespace password_manager
