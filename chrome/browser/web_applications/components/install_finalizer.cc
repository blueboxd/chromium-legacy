// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_finalizer.h"

#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

namespace web_app {

void InstallFinalizer::SetSubsystems(WebAppUiManager* ui_manager) {
  ui_manager_ = ui_manager;
}

bool InstallFinalizer::CanAddAppToQuickLaunchBar() const {
  return ui_manager().CanAddAppToQuickLaunchBar();
}

void InstallFinalizer::AddAppToQuickLaunchBar(const AppId& app_id) {
  ui_manager().AddAppToQuickLaunchBar(app_id);
}

}  // namespace web_app
