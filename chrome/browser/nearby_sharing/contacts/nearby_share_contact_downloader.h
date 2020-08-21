// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

// Makes RPC calls to check if the user's contact list has changed since the
// last contact upload to the server, and downloads the contact list if
// necessary. NOTE: An instance should only be used once. All necessary
// parameters are passed to the constructor, and the download begins when Run()
// is called.
class NearbyShareContactDownloader {
 public:
  // |did_contacts_change_since_last_upload|: True if the Nearby Share server
  //                                          determines that the user's contact
  //                                          list has changed since the last
  //                                          contact upload to the server.
  // |contacts|: The user's complete list of contacts or base::nullopt if the
  //             user requested a download only if the server indicated that the
  //             contact list changed.
  using SuccessCallback = base::OnceCallback<void(
      bool did_contacts_change_since_last_upload,
      base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts)>;

  using FailureCallback = base::OnceClosure;

  // |only_download_if_changed|: When true, contacts will only be downloaded if
  //                             the Nearby Share server determines that the
  //                             user's contact list has changed. When false,
  //                             the contact list will always be retrieved.
  // |device_id|: The ID used by the Nearby server to differentiate multiple
  //              devices from the same account.
  // |success_callback|: Invoked if the contact-change check and possibly the
  //                     contact list download finishes successfully.
  // |failure_callback|: Invoked if the contact-change check or the contact list
  //                     download fails.
  NearbyShareContactDownloader(bool only_download_if_changed,
                               const std::string& device_id,
                               SuccessCallback success_callback,
                               FailureCallback failure_callback);

  virtual ~NearbyShareContactDownloader();

  // Runs the contact-change check and subsequent contact list download if
  // necessary.
  void Run();

 protected:
  bool only_download_if_changed() const { return only_download_if_changed_; }
  const std::string& device_id() const { return device_id_; }

  virtual void OnRun() = 0;

  // Invokes the success callback with the input parameters.
  void Succeed(
      bool did_contacts_change_since_last_upload,
      base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts);

  // Invokes the failure callback.
  void Fail();

 private:
  bool was_run_ = false;
  const bool only_download_if_changed_;
  const std::string device_id_;
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
