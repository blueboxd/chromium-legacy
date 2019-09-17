// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_sync_bridge.h"

#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

TestWebAppSyncBridge::TestWebAppSyncBridge() {}

TestWebAppSyncBridge::~TestWebAppSyncBridge() {}

void TestWebAppSyncBridge::OpenDatabase(RegistryOpenedCallback callback) {
  open_database_callback_ = std::move(callback);
}

void TestWebAppSyncBridge::WriteWebApps(AppsToWrite apps,
                                        CompletionCallback callback) {
  for (auto* app : apps)
    write_web_app_ids_.push_back(app->app_id());

  std::move(callback).Run(next_write_web_apps_result_);
}

void TestWebAppSyncBridge::DeleteWebApps(AppIds app_ids,
                                         CompletionCallback callback) {
  delete_web_app_ids_ = std::move(app_ids);

  std::move(callback).Run(next_delete_web_apps_result_);
}

void TestWebAppSyncBridge::SetNextWriteWebAppsResult(
    bool next_write_web_apps_result) {
  next_write_web_apps_result_ = next_write_web_apps_result;
}

void TestWebAppSyncBridge::SetNextDeleteWebAppsResult(
    bool next_delete_web_apps_result) {
  next_delete_web_apps_result_ = next_delete_web_apps_result;
}

}  // namespace web_app
