// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_MODULE_PROVIDER_H_
#define CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_MODULE_PROVIDER_H_

#include "base/callback.h"

namespace dev_ui {

class DevUiModuleProvider {
 public:
  // Returns true if the DevUI module is installed.
  static bool ModuleInstalled();

  // Asynchronously requests to install the DevUI module. |on_complete| is
  // called after the module install is completed, and takes a bool to indicate
  // whether module install is successful.
  static void InstallModule(base::OnceCallback<void(bool)> on_complete);

  // Assuming that the DevUI module is installed, loads DevUI resources if not
  // already loaded.
  static void LoadModule();

 private:
  DevUiModuleProvider();
  ~DevUiModuleProvider();
  DevUiModuleProvider(const DevUiModuleProvider&) = delete;
  DevUiModuleProvider& operator=(const DevUiModuleProvider&) = delete;
};

}  // namespace dev_ui

#endif  // CHROME_ANDROID_MODULES_DEV_UI_PROVIDER_DEV_UI_MODULE_PROVIDER_H_
