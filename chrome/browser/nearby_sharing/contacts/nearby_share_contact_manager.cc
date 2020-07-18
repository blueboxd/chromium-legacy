// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"

NearbyShareContactManager::NearbyShareContactManager() = default;

NearbyShareContactManager::~NearbyShareContactManager() = default;

void NearbyShareContactManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareContactManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareContactManager::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  OnStart();
}

void NearbyShareContactManager::Stop() {
  DCHECK(is_running_);
  is_running_ = false;
  OnStop();
}

void NearbyShareContactManager::NotifyContactsUpdated(
    bool contacts_list_changed,
    bool contacts_added_to_allowlist,
    bool contacts_removed_from_allowlist,
    const std::set<std::string>& allowed_contact_ids,
    const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
        contacts) {
  for (auto& observer : observers_) {
    observer.OnContactsUpdated(
        contacts_list_changed, contacts_added_to_allowlist,
        contacts_removed_from_allowlist, allowed_contact_ids, contacts);
  }
}
