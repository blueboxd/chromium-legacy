// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

class NearbySharingServiceImpl : public NearbySharingService,
                                 public KeyedService {
 public:
  explicit NearbySharingServiceImpl(Profile* profile);
  ~NearbySharingServiceImpl() override = default;

  // NearbySharingService:
  void RegisterSendSurface(TransferUpdateCallback* transferCallback,
                           ShareTargetDiscoveredCallback* discoveryCallback,
                           StatusCodesCallback status_codes_callback) override;
  void UnregisterSendSurface(
      TransferUpdateCallback* transferCallback,
      ShareTargetDiscoveredCallback* discoveryCallback,
      StatusCodesCallback status_codes_callback) override;
  void RegisterReceiveSurface(
      TransferUpdateCallback* transferCallback,
      StatusCodesCallback status_codes_callback) override;
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transferCallback,
      StatusCodesCallback status_codes_callback) override;
  void SendText(const ShareTarget& share_target,
                std::string text,
                StatusCodesCallback status_codes_callback) override;
  void SendFiles(const ShareTarget& share_target,
                 const std::vector<base::FilePath>& files,
                 StatusCodesCallback status_codes_callback) override;
  void Accept(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Reject(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Cancel(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Open(const ShareTarget& share_target,
            StatusCodesCallback status_codes_callback) override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
