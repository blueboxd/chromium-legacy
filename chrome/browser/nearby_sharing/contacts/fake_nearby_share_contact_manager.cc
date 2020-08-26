// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_manager.h"

FakeNearbyShareContactManager::Factory::Factory() = default;

FakeNearbyShareContactManager::Factory::~Factory() = default;

std::unique_ptr<NearbyShareContactManager>
FakeNearbyShareContactManager::Factory::CreateInstance() {
  auto instance = std::make_unique<FakeNearbyShareContactManager>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareContactManager::FakeNearbyShareContactManager() = default;

FakeNearbyShareContactManager::~FakeNearbyShareContactManager() = default;

void FakeNearbyShareContactManager::DownloadContacts(
    bool only_download_if_changed) {
  download_contacts_calls_.push_back(only_download_if_changed);
}

void FakeNearbyShareContactManager::SetAllowedContacts(
    const std::set<std::string>& allowed_contact_ids) {
  set_allowed_contacts_calls_.push_back(allowed_contact_ids);
}

void FakeNearbyShareContactManager::OnStart() {}

void FakeNearbyShareContactManager::OnStop() {}
