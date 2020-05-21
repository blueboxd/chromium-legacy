// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/nearby_sharing/share_target_discovered_callback.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"

// This service implements Nearby Sharing on top of the Nearby Connections mojo.
// Currently only single profile will be allowed to be bound at a time and only
// after the user has enabled Nearby Sharing in prefs.
class NearbySharingService {
 public:
  enum class StatusCodes {
    // The operation was successful.
    kOk,
    // The operation failed, without any more information.
    kError,
  };

  using StatusCodesCallback =
      base::OnceCallback<void(StatusCodes status_codes)>;

  virtual ~NearbySharingService() = default;

  // Registers a send surface for handling payload transfer status and device
  // discovery.
  virtual void RegisterSendSurface(
      TransferUpdateCallback* transferCallback,
      ShareTargetDiscoveredCallback* discoveryCallback,
      StatusCodesCallback status_codes_callback) = 0;

  // Unregisters the current send surface.
  virtual void UnregisterSendSurface(
      TransferUpdateCallback* transferCallback,
      ShareTargetDiscoveredCallback* discoveryCallback,
      StatusCodesCallback status_codes_callback) = 0;

  // Registers a receiver surface for handling payload transfer status.
  virtual void RegisterReceiveSurface(
      TransferUpdateCallback* transferCallback,
      StatusCodesCallback status_codes_callback) = 0;

  // Unregistesrs the current receive surface.
  virtual void UnregisterReceiveSurface(
      TransferUpdateCallback* transferCallback,
      StatusCodesCallback status_codes_callback) = 0;

  // Sends text to the remote |share_target|.
  virtual void SendText(const ShareTarget& share_target,
                        std::string text,
                        StatusCodesCallback status_codes_callback) = 0;

  // Sends files to the remote |share_target|.
  virtual void SendFiles(const ShareTarget& share_target,
                         const std::vector<base::FilePath>& files,
                         StatusCodesCallback status_codes_callback) = 0;

  // Accepts incoming share from the remote |share_target|.
  virtual void Accept(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Rejects incoming share from the remote |share_target|.
  virtual void Reject(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Cancels outoing shares to the remote |share_target|.
  virtual void Cancel(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Opens attachments from the remote |share_target|.
  virtual void Open(const ShareTarget& share_target,
                    StatusCodesCallback status_codes_callback) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
