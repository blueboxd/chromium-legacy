// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include "base/logging.h"

NearbySharingServiceImpl::NearbySharingServiceImpl(Profile* profile) {}

void NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transferCallback,
    ShareTargetDiscoveredCallback* discoveryCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transferCallback,
    ShareTargetDiscoveredCallback* discoveryCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transferCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transferCallback,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::NearbySharingServiceImpl::SendText(
    const ShareTarget& share_target,
    std::string text,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::SendFiles(
    const ShareTarget& share_target,
    const std::vector<base::FilePath>& files,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Accept(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Reject(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Cancel(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Open(const ShareTarget& share_target,
                                    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}
