// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_background_task.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"

namespace upboarding {

TileBackgroundTask::TileBackgroundTask() = default;

TileBackgroundTask::~TileBackgroundTask() = default;

void TileBackgroundTask::OnStartTaskInReducedMode(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  callback_ = std::move(callback);
}

void TileBackgroundTask::OnStartTaskWithFullBrowser(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  auto* profile_key =
      Profile::FromBrowserContext(browser_context)->GetProfileKey();
  StartFetchTask(profile_key, false, std::move(callback));
}

void TileBackgroundTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  // TODO(hesen): CancelTask and return if feature is disabled.
  auto* profile_key =
      Profile::FromBrowserContext(browser_context)->GetProfileKey();
  StartFetchTask(profile_key, false, std::move(callback_));
}

bool TileBackgroundTask::OnStopTask(const TaskParameters& task_params) {
  // Don't reschedule.
  return false;
}

void TileBackgroundTask::StartFetchTask(SimpleFactoryKey* key,
                                        bool is_from_reduced_mode,
                                        TaskFinishedCallback callback) {
  if (is_from_reduced_mode)
    return;
  auto* tile_service =
      upboarding::TileServiceFactory::GetInstance()->GetForKey(key);
  DCHECK(tile_service);
  tile_service->StartFetchForTiles(std::move(callback));
}

}  // namespace upboarding
