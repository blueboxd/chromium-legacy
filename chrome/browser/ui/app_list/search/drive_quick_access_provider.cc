// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_provider.h"

#include <memory>

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"

namespace app_list {
namespace {

constexpr int kMaxItems = 5;

}

DriveQuickAccessProvider::DriveQuickAccessProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)),
      weak_factory_(this) {
  DCHECK(profile_);
}

DriveQuickAccessProvider::~DriveQuickAccessProvider() = default;

void DriveQuickAccessProvider::Start(const base::string16& query) {
  // TODO(crbug.com/959679): Add latency metrics.
  ClearResultsSilently();
  if (!query.empty() || !drive_service_)
    return;
  drive_service_->GetQuickAccessItems(
      kMaxItems,
      base::BindOnce(&DriveQuickAccessProvider::OnGetQuickAccessItems,
                     weak_factory_.GetWeakPtr()));
}

void DriveQuickAccessProvider::OnGetQuickAccessItems(
    drive::FileError error,
    std::vector<drive::QuickAccessItem> drive_results) {
  // Results are launched via DriveFS, so DriveFS must be mounted.
  if (!drive_service_ || !drive_service_->IsMounted())
    return;

  // TODO(crbug.com/959679): Add score distribution metrics.
  SearchProvider::Results results;
  for (const auto& result : drive_results) {
    results.emplace_back(std::make_unique<DriveQuickAccessResult>(
        drive_service_->GetMountPointPath().Append(result.path),
        result.confidence, profile_));
  }
  SwapResults(&results);
}

}  // namespace app_list
