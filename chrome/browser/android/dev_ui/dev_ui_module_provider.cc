// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_ui/dev_ui_module_provider.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/android/modules/dev_ui/provider/jni_headers/DevUiModuleProvider_jni.h"
#include "chrome/browser/android/dfm_resource_bundle_helper.h"

namespace dev_ui {

namespace {

constexpr base::TaskTraits kWorkerTaskTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

// static
DevUiModuleProvider& DevUiModuleProvider::GetInstance() {
  static base::NoDestructor<DevUiModuleProvider> instance;
  return *instance;
}

bool DevUiModuleProvider::ModuleInstalled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Java_DevUiModuleProvider_isModuleInstalled(
      base::android::AttachCurrentThread());
}

bool DevUiModuleProvider::ModuleLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DevUiModuleProvider::GetInstance().is_loaded_;
}

void DevUiModuleProvider::InstallModule(
    base::OnceCallback<void(bool)> on_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Also use the call's side effect of ensuring that split compat is loaded.
  // TODO(tiborg): Once ModuleInstalled() is made more light-weight, add
  // explicit split compat loading code here.
  if (ModuleInstalled()) {
    std::move(on_finished).Run(true);
    return;
  }
  on_attempted_install_callbacks_.push(std::move(on_finished));
  // Don't request DevUI module multiple times in parallel.
  if (on_attempted_install_callbacks_.size() > 1)
    return;

  // This should always return, since there is no InfoBar UI to retry (thus
  // avoiding crbug.com/996925 and crbug.com/996959).
  Java_DevUiModuleProvider_installModule(base::android::AttachCurrentThread(),
                                         j_dev_ui_module_provider_);
}

// Called by Java.
void DevUiModuleProvider::OnInstallResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(on_attempted_install_callbacks_.size(), 0UL);
  while (!on_attempted_install_callbacks_.empty()) {
    std::move(on_attempted_install_callbacks_.front()).Run(success);
    on_attempted_install_callbacks_.pop();
  }
}

void DevUiModuleProvider::LoadModule(base::OnceCallback<void()> on_loaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ModuleLoaded()) {
    std::move(on_loaded).Run();
    return;
  }
  on_loaded_callbacks_.push(std::move(on_loaded));
  // Don't load multiple times in parallel.
  if (on_loaded_callbacks_.size() > 1)
    return;
  // android::LoadDevUiResources() is assumed to always succeed.
  base::PostTaskAndReply(FROM_HERE, kWorkerTaskTraits,
                         base::BindOnce(&android::LoadDevUiResources),
                         base::BindOnce(&DevUiModuleProvider::OnLoadedModule,
                                        base::Unretained(this)));
}

DevUiModuleProvider::DevUiModuleProvider()
    : j_dev_ui_module_provider_(Java_DevUiModuleProvider_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<jlong>(this))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DevUiModuleProvider::~DevUiModuleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_DevUiModuleProvider_onNativeDestroy(base::android::AttachCurrentThread(),
                                           j_dev_ui_module_provider_);
}

void DevUiModuleProvider::OnLoadedModule() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loaded_ = true;
  DCHECK_GT(on_loaded_callbacks_.size(), 0UL);
  while (!on_loaded_callbacks_.empty()) {
    std::move(on_loaded_callbacks_.front()).Run();
    on_loaded_callbacks_.pop();
  }
}

}  // namespace dev_ui
