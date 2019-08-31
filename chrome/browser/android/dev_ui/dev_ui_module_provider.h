// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_MODULE_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_MODULE_PROVIDER_H_

#include <jni.h>
#include <queue>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace dev_ui {

class DevUiModuleProvider {
 public:
  static DevUiModuleProvider& GetInstance();

  // Returns true if the DevUI module is installed.
  bool ModuleInstalled();

  // Returns true if the DevUI module is installed and loaded.
  bool ModuleLoaded();

  // Asynchronously requests to install the DevUI module. |on_finished| is
  // called after the module install is completed, and takes a bool to indicate
  // whether module install is successful.
  void InstallModule(base::OnceCallback<void(bool)> on_finished);

  // Called by Java.
  void OnInstallResult(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       bool success);

  // Assuming that the DevUI module is installed, asynchronously loads DevUI
  // resources.
  void LoadModule(base::OnceCallback<void()> on_loaded);

 private:
  friend base::NoDestructor<DevUiModuleProvider>;
  DevUiModuleProvider();
  ~DevUiModuleProvider();
  DevUiModuleProvider(const DevUiModuleProvider&) = delete;
  void operator=(const DevUiModuleProvider&) = delete;

  // Callback for LoadModule().
  void OnLoadedModule();

  base::android::ScopedJavaGlobalRef<jobject> j_dev_ui_module_provider_;

  // Queue of InstallModule() callbacks.
  std::queue<base::OnceCallback<void(bool)>> on_attempted_install_callbacks_;

  // Queue of LoadModule() callbacks.
  std::queue<base::OnceCallback<void()>> on_loaded_callbacks_;

  // Source of truth for ModuleLoaded(), updated when LoadModule() succeeds.
  bool is_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace dev_ui

#endif  // CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_MODULE_PROVIDER_H_
