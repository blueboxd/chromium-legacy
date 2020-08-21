// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"

#include <utility>

#include "base/check.h"

namespace {

void RecordSuccessMetrics(
    bool did_contacts_change_since_last_upload,
    const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
        contacts) {
  // TODO(https://crbug.com/1105579): Record a success/failure histogram value.
  // TODO(https://crbug.com/1105579): Record if contacts changed.
  // TODO(https://crbug.com/1105579): Record if contacts were downloaded.
  // TODO(https://crbug.com/1105579): If contacts were downloaded, record total
  // number of contacts returned.
}

void RecordFailureMetrics() {
  // TODO(https://crbug.com/1105579): Record a success/failure histogram value.
}

}  // namespace

NearbyShareContactDownloader::NearbyShareContactDownloader(
    bool only_download_if_changed,
    const std::string& device_id,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : only_download_if_changed_(only_download_if_changed),
      device_id_(device_id),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {}

NearbyShareContactDownloader::~NearbyShareContactDownloader() = default;

void NearbyShareContactDownloader::Run() {
  DCHECK(!was_run_);
  was_run_ = true;

  OnRun();
}

void NearbyShareContactDownloader::Succeed(
    bool did_contacts_change_since_last_upload,
    base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts) {
  DCHECK(was_run_);
  DCHECK(success_callback_);
  RecordSuccessMetrics(did_contacts_change_since_last_upload, contacts);

  std::move(success_callback_)
      .Run(did_contacts_change_since_last_upload, std::move(contacts));
}

void NearbyShareContactDownloader::Fail() {
  DCHECK(was_run_);
  DCHECK(failure_callback_);
  RecordFailureMetrics();

  std::move(failure_callback_).Run();
}
