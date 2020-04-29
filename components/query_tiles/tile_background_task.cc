// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile_background_task.h"

#include "base/macros.h"

namespace upboarding {

TileBackgroundTask::TileBackgroundTask() = default;

TileBackgroundTask::~TileBackgroundTask() = default;

void TileBackgroundTask::OnStartTaskInReducedMode(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  NOTIMPLEMENTED();
}

void TileBackgroundTask::OnStartTaskWithFullBrowser(
    const TaskParameters& task_params,
    TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  NOTIMPLEMENTED();
}

void TileBackgroundTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  NOTIMPLEMENTED();
}

bool TileBackgroundTask::OnStopTask(const TaskParameters& task_params) {
  NOTIMPLEMENTED();
  return true;
}

}  // namespace upboarding
