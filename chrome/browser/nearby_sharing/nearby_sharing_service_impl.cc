// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_prefs.h"
#include "components/prefs/pref_service.h"

NearbySharingServiceImpl::NearbySharingServiceImpl(
    PrefService* prefs,
    Profile* profile,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager)
    : prefs_(prefs),
      profile_(profile),
      nearby_connections_manager_(std::move(nearby_connections_manager)) {
  DCHECK(prefs_);
  DCHECK(profile_);
  DCHECK(nearby_connections_manager_);

  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  nearby_process_observer_.Add(&process_manager);

  if (process_manager.IsActiveProfile(profile_)) {
    // TODO(crbug.com/1084576): Initialize NearbyConnectionsManager with
    // NearbyConnectionsMojom from |process_manager|:
    // process_manager.GetOrStartNearbyConnections(profile_)
  }

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kNearbySharingEnabledPrefName,
      base::BindRepeating(&NearbySharingServiceImpl::OnEnabledPrefChanged,
                          base::Unretained(this)));
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() = default;

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

void NearbySharingServiceImpl::OnNearbyProfileChanged(Profile* profile) {
  // TODO(crbug.com/1084576): Notify UI about the new active profile.
}

void NearbySharingServiceImpl::OnNearbyProcessStarted() {
  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  if (process_manager.IsActiveProfile(profile_))
    VLOG(1) << __func__ << ": Nearby process started!";
}

void NearbySharingServiceImpl::OnNearbyProcessStopped() {
  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();
  if (process_manager.IsActiveProfile(profile_)) {
    // TODO(crbug.com/1084576): Check if process should be running and restart
    // it after a delay.
  }
}

bool NearbySharingServiceImpl::IsEnabled() {
  return prefs_->GetBoolean(prefs::kNearbySharingEnabledPrefName);
}

void NearbySharingServiceImpl::OnEnabledPrefChanged() {
  if (IsEnabled()) {
    VLOG(1) << __func__ << ": Nearby sharing enabled!";
  } else {
    VLOG(1) << __func__ << ": Nearby sharing disabled!";
    // TODO(crbug/1084647): Stop advertising.
    // TODO(crbug/1085067): Stop discovery.
    nearby_connections_manager_->Shutdown();
  }
}
