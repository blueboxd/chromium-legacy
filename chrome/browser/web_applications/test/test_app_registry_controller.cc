// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_app_registry_controller.h"

namespace web_app {

TestAppRegistryController::TestAppRegistryController(Profile* profile)
    : AppRegistryController(profile) {}
TestAppRegistryController::~TestAppRegistryController() = default;

void TestAppRegistryController::Init(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TestAppRegistryController::SetAppUserDisplayMode(
    const AppId& app_id,
    DisplayMode display_mode) {}

void TestAppRegistryController::SetAppIsDisabled(const AppId& app_id,
                                                 bool is_disabled) {}

void TestAppRegistryController::SetAppIsLocallyInstalled(
    const AppId& app_id,
    bool is_locally_installed) {}

WebAppSyncBridge* TestAppRegistryController::AsWebAppSyncBridge() {
  return nullptr;
}

}  // namespace web_app
