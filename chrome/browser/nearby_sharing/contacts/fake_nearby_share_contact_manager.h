// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

// A fake implementation of NearbyShareContactManager, along with a fake
// factory, to be used in tests. Stores parameters input into
// NearbyShareContactManager method calls. Provides a method to notify
// observers.
class FakeNearbyShareContactManager : public NearbyShareContactManager {
 public:
  // Factory that creates FakeNearbyShareContactManager instances. Use in
  // NearbyShareContactManagerImpl::Factor::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareContactManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareContactManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareContactManager*>& instances() {
      return instances_;
    }

   private:
    // NearbyShareContactManagerImpl::Factory:
    std::unique_ptr<NearbyShareContactManager> CreateInstance() override;

    std::vector<FakeNearbyShareContactManager*> instances_;
  };

  FakeNearbyShareContactManager();
  ~FakeNearbyShareContactManager() override;

  void NotifyObservers(
      bool contacts_list_changed,
      bool contacts_added_to_allowlist,
      bool contacts_removed_from_allowlist,
      const std::set<std::string>& allowed_contact_ids,
      const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
          contacts);

  // Returns inputs of all DownloadContacts() calls.
  const std::vector<bool>& download_contacts_calls() const {
    return download_contacts_calls_;
  }

  // Returns inputs of all SetAllowedContacts() calls.
  const std::vector<std::set<std::string>>& set_allowed_contacts_calls() const {
    return set_allowed_contacts_calls_;
  }

 private:
  // NearbyShareContactsManager:
  void DownloadContacts(bool only_download_if_changed) override;
  void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) override;
  void OnStart() override;
  void OnStop() override;

  std::vector<bool> download_contacts_calls_;
  std::vector<std::set<std::string>> set_allowed_contacts_calls_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_MANAGER_H_
