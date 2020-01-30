// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/android/vr/android_vr_utils.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreInstallUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"

using base::android::AttachCurrentThread;

namespace vr {

namespace {
class ArCoreDeviceProviderFactoryImpl
    : public device::ArCoreDeviceProviderFactory {
 public:
  ArCoreDeviceProviderFactoryImpl() = default;
  ~ArCoreDeviceProviderFactoryImpl() override = default;
  std::unique_ptr<device::VRDeviceProvider> CreateDeviceProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProviderFactoryImpl);
};

std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactoryImpl::CreateDeviceProvider() {
  return std::make_unique<device::ArCoreDeviceProvider>();
}

}  // namespace

ArCoreInstallHelper::ArCoreInstallHelper()
    : XrInstallHelper(), weak_ptr_factory_(this) {}

ArCoreInstallHelper::~ArCoreInstallHelper() {
  if (!java_install_utils_.is_null()) {
    Java_ArCoreInstallUtils_onNativeDestroy(AttachCurrentThread(),
                                            java_install_utils_);
  }

  RunInstallFinishedCallback(false);
}

void ArCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    OnInstallFinishedCallback install_callback) {
  DCHECK(!install_finished_callback_);
  install_finished_callback_ = std::move(install_callback);
  render_process_id_ = render_process_id;
  render_frame_id_ = render_frame_id;

  java_install_utils_ = Java_ArCoreInstallUtils_create(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));

  if (java_install_utils_.is_null()) {
    RunInstallFinishedCallback(false);
    return;
  }

  RequestArModule();
}

bool ArCoreInstallHelper::CanRequestInstallArModule() {
  return Java_ArCoreInstallUtils_canRequestInstallArModule(
      AttachCurrentThread(), java_install_utils_);
}

bool ArCoreInstallHelper::ShouldRequestInstallArModule() {
  return Java_ArCoreInstallUtils_shouldRequestInstallArModule(
      AttachCurrentThread(), java_install_utils_);
}

void ArCoreInstallHelper::RequestInstallArModule() {
  Java_ArCoreInstallUtils_requestInstallArModule(
      AttachCurrentThread(), java_install_utils_,
      GetTabFromRenderer(render_process_id_, render_frame_id_));
}

bool ArCoreInstallHelper::ShouldRequestInstallSupportedArCore() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
      env, java_install_utils_);
}

void ArCoreInstallHelper::RequestInstallSupportedArCore() {
  DCHECK(ShouldRequestInstallSupportedArCore());

  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreInstallUtils_requestInstallSupportedArCore(
      env, java_install_utils_,
      GetTabFromRenderer(render_process_id_, render_frame_id_));
}

void ArCoreInstallHelper::OnRequestInstallArModuleResult(JNIEnv* env,
                                                         bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (on_request_ar_module_result_callback_) {
    std::move(on_request_ar_module_result_callback_).Run(success);
  }
}

void ArCoreInstallHelper::OnRequestInstallSupportedArCoreResult(JNIEnv* env,
                                                                bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(on_request_arcore_install_or_update_result_callback_);

  std::move(on_request_arcore_install_or_update_result_callback_).Run(success);
}

void ArCoreInstallHelper::RequestArModule() {
  DVLOG(1) << __func__;

  if (ShouldRequestInstallArModule()) {
    // AR DFM is disabled - if we think we should install AR module, then it
    // means that we are using a build that does not support AR capabilities.
    // Treat this as if the AR module installation failed.
    LOG(WARNING) << "AR is not supported on this build";

    OnRequestArModuleResult(false);
    return;
  }

  OnRequestArModuleResult(true);
}

void ArCoreInstallHelper::OnRequestArModuleResult(bool success) {
  DVLOG(3) << __func__ << ": success=" << success;

  if (!success) {
    RunInstallFinishedCallback(false);
    return;
  }

  RequestArCoreInstallOrUpdate();
}

void ArCoreInstallHelper::RequestArCoreInstallOrUpdate() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!on_request_arcore_install_or_update_result_callback_);

  if (ShouldRequestInstallSupportedArCore()) {
    // ARCore is not installed or requires an update. Store the callback to be
    // processed later once installation/update is complete or got cancelled.
    on_request_arcore_install_or_update_result_callback_ = base::BindOnce(
        &ArCoreInstallHelper::OnRequestArCoreInstallOrUpdateResult,
        GetWeakPtr());

    RequestInstallSupportedArCore();
    return;
  }

  OnRequestArCoreInstallOrUpdateResult(true);
}

void ArCoreInstallHelper::OnRequestArCoreInstallOrUpdateResult(bool success) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunInstallFinishedCallback(success);
}

void ArCoreInstallHelper::RunInstallFinishedCallback(bool succeeded) {
  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(succeeded);
  }
}

static void JNI_ArCoreInstallUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
