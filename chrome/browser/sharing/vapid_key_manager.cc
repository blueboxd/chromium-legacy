// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/vapid_key_manager.h"

#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "crypto/ec_private_key.h"

VapidKeyManager::VapidKeyManager(SharingSyncPreference* sharing_sync_preference)
    : sharing_sync_preference_(sharing_sync_preference) {}

VapidKeyManager::~VapidKeyManager() = default;

crypto::ECPrivateKey* VapidKeyManager::GetOrCreateKey() {
  base::Optional<std::vector<uint8_t>> stored_key =
      sharing_sync_preference_->GetVapidKey();

  if (stored_key) {
    vapid_key_ = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(*stored_key);
    return vapid_key_.get();
  }

  vapid_key_ = crypto::ECPrivateKey::Create();
  if (!vapid_key_) {
    LogSharingVapidKeyCreationResult(
        SharingVapidKeyCreationResult::kGenerateECKeyFailed);
    return nullptr;
  }

  std::vector<uint8_t> key;
  if (!vapid_key_->ExportPrivateKey(&key)) {
    LOG(ERROR) << "Could not export vapid key";
    vapid_key_.reset();
    LogSharingVapidKeyCreationResult(
        SharingVapidKeyCreationResult::kExportPrivateKeyFailed);
    return nullptr;
  }

  sharing_sync_preference_->SetVapidKey(key);
  LogSharingVapidKeyCreationResult(SharingVapidKeyCreationResult::kSuccess);
  return vapid_key_.get();
}
