// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"

namespace app_list {
namespace {

constexpr int kMaxItems = 5;

// Given a vector of QuickAccessItems, return only those that exist on-disk.
std::vector<drive::QuickAccessItem> FilterResults(
    const drive::DriveIntegrationService* drive_service,
    const std::vector<drive::QuickAccessItem>& drive_results) {
  std::vector<drive::QuickAccessItem> valid_results;
  for (const auto& result : drive_results) {
    if (base::PathExists(
            drive_service->GetMountPointPath().Append(result.path))) {
      valid_results.emplace_back(result);
    }
  }
  return valid_results;
}

}  // namespace

DriveQuickAccessProvider::DriveQuickAccessProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Warm up the cache.
  AppListShown();
}

DriveQuickAccessProvider::~DriveQuickAccessProvider() = default;

void DriveQuickAccessProvider::Start(const base::string16& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();
  ClearResultsSilently();
  // Results are launched via DriveFS, so DriveFS must be mounted.
  if (!query.empty() || !drive_service_ || !drive_service_->IsMounted())
    return;

  // If there are no items in the cache, the previous call may have failed so
  // retry. We return no results in this case, because waiting for the new
  // results would introduce too much latency.
  if (results_cache_.empty()) {
    GetQuickAccessItems();
    return;
  }

  SearchProvider::Results results;
  for (const auto& result : results_cache_) {
    results.emplace_back(std::make_unique<DriveQuickAccessResult>(
        drive_service_->GetMountPointPath().Append(result.path),
        result.confidence, profile_));
  }
  UMA_HISTOGRAM_TIMES("Apps.AppList.DriveQuickAccessProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
  SwapResults(&results);
}

void DriveQuickAccessProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!drive_service_)
    return;
  GetQuickAccessItems();
}

void DriveQuickAccessProvider::GetQuickAccessItems() {
  drive_service_->GetQuickAccessItems(
      kMaxItems,
      base::BindOnce(&DriveQuickAccessProvider::OnGetQuickAccessItems,
                     weak_factory_.GetWeakPtr()));
}

void DriveQuickAccessProvider::OnGetQuickAccessItems(
    drive::FileError error,
    std::vector<drive::QuickAccessItem> drive_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // An empty |drive_results| is likely caused by a failed call to ItemSuggest,
  // so don't replace the cache.
  if (!drive_results.empty()) {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&FilterResults, drive_service_, drive_results),
        base::BindOnce(&DriveQuickAccessProvider::SetResultsCache,
                       weak_factory_.GetWeakPtr()));
  }
}

void DriveQuickAccessProvider::SetResultsCache(
    const std::vector<drive::QuickAccessItem>& drive_results) {
  results_cache_ = std::move(drive_results);
}

}  // namespace app_list
