// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

// The Nearby Share contacts manager interfaces with the Nearby server to 1)
// download the user's contacts and 2) upload the user-input list of allowed
// contacts for selected-contacts visibility mode. All contact data and update
// notifications are conveyed to observers via OnContactsUpdated(); the manager
// does not return data directly from function calls.
class NearbyShareContactManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnContactsUpdated(
        bool contacts_list_changed,
        bool contacts_added_to_allowlist,
        bool contacts_removed_from_allowlist,
        const std::set<std::string>& allowed_contact_ids,
        const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
            contacts) = 0;
  };

  NearbyShareContactManager();
  virtual ~NearbyShareContactManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops contact task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // Makes RPC calls to check if the user's contact list has changed since the
  // last call to the server. If it changed or if |only_download_if_changed| is
  // false, the contact list is downloaded from the server. The list of allowed
  // contacts is reconciled with the newly downloaded contacts. These RPC calls
  // are also scheduled periodically. The results are sent to observers via
  // OnContactsUpdated().
  virtual void DownloadContacts(bool only_download_if_changed) = 0;

  // Assigns the set of contacts that the local device allows sharing with when
  // in selected-contacts visibility mode. (Note: This set is irrelevant for
  // all-contacts visibility mode.) The allowed contact list determines what
  // contacts receive the local device's "selected-contacts" visibility public
  // certificates. Changes to the allowlist will trigger an RPC call. Observers
  // are notified of any changes to the allowlist via OnContactsUpdated().
  virtual void SetAllowedContacts(
      const std::set<std::string>& allowed_contact_ids) = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyContactsUpdated(
      bool contacts_list_changed,
      bool contacts_added_to_allowlist,
      bool contacts_removed_from_allowlist,
      const std::set<std::string>& allowed_contact_ids,
      const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
          contacts);

 private:
  bool is_running_ = false;
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_MANAGER_H_
