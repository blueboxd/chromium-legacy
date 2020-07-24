// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager.h"

// TODO(nohle): Add description after class is fully implemented.
class NearbyShareContactManagerImpl : public NearbyShareContactManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareContactManager> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareContactManager> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareContactManagerImpl() override;

 private:
  NearbyShareContactManagerImpl();

  // NearbyShareContactsManager:
  void DownloadContacts(bool only_download_if_changed) override;
  void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) override;
  void OnStart() override;
  void OnStop() override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_IMPL_H_
