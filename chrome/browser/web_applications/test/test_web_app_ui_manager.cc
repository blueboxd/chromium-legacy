// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"

#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace web_app {

TestWebAppUiManager::TestWebAppUiManager() = default;

TestWebAppUiManager::~TestWebAppUiManager() = default;

WebAppDialogManager& TestWebAppUiManager::dialog_manager() {
  // TODO(crbug.com/973324): Implement a TestWebAppDialogManager to return here.
  NOTIMPLEMENTED();
  static WebAppDialogManager* manager = nullptr;
  return *manager;
}

void TestWebAppUiManager::SetNumWindowsForApp(const AppId& app_id,
                                              size_t num_windows_for_app) {
  app_id_to_num_windows_map_[app_id] = num_windows_for_app;
}

size_t TestWebAppUiManager::GetNumWindowsForApp(const AppId& app_id) {
  DCHECK(base::Contains(app_id_to_num_windows_map_, app_id));
  return app_id_to_num_windows_map_[app_id];
}

void TestWebAppUiManager::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&, app_id, callback = std::move(callback)]() mutable {
                       app_id_to_num_windows_map_[app_id] = 0;
                       std::move(callback).Run();
                     }));
}

}  // namespace web_app
