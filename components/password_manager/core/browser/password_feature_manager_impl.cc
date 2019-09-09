// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_feature_manager_impl.h"

#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/sync/driver/sync_service.h"

namespace password_manager {

PasswordFeatureManagerImpl::PasswordFeatureManagerImpl(
    const syncer::SyncService* sync_service)
    : sync_service_(sync_service) {}

bool PasswordFeatureManagerImpl::IsGenerationEnabled() const {
  switch (password_manager_util::GetPasswordSyncState(sync_service_)) {
    case NOT_SYNCING:
      return false;
    case SYNCING_WITH_CUSTOM_PASSPHRASE:
    case SYNCING_NORMAL_ENCRYPTION:
      return true;
  }
}

}  // namespace password_manager
