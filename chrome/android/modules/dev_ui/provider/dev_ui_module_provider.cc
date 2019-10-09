// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/dev_ui/provider/dev_ui_module_provider.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/android/modules/dev_ui/provider/dev_ui_install_listener.h"
#include "chrome/android/modules/dev_ui/provider/jni_headers/DevUiModuleProvider_jni.h"

namespace dev_ui {

// static
bool DevUiModuleProvider::ModuleInstalled() {
  return Java_DevUiModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

// static
void DevUiModuleProvider::InstallModule(
    base::OnceCallback<void(bool)> on_complete) {
  auto* listener = DevUiInstallListener::Create(std::move(on_complete));
  // This should always return, since there is no InfoBar UI to retry (thus
  // avoiding crbug.com/996925 and crbug.com/996959).
  Java_DevUiModuleProvider_installModule(base::android::AttachCurrentThread(),
                                         listener->j_listener());
}

// static
void DevUiModuleProvider::LoadModule() {
  Java_DevUiModuleProvider_loadModule(base::android::AttachCurrentThread());
}

DevUiModuleProvider::DevUiModuleProvider() = default;

DevUiModuleProvider::~DevUiModuleProvider() = default;

}  // namespace dev_ui
