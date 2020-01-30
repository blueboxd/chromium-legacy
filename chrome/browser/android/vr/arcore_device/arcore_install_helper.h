// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/vr/service/xr_install_helper.h"
#include "chrome/browser/vr/vr_export.h"

namespace vr {

class VR_EXPORT ArCoreInstallHelper : public XrInstallHelper {
 public:
  ArCoreInstallHelper();
  ~ArCoreInstallHelper() override;

  ArCoreInstallHelper(const ArCoreInstallHelper&) = delete;
  ArCoreInstallHelper& operator=(const ArCoreInstallHelper&) = delete;

  void EnsureInstalled(int render_process_id,
                       int render_frame_id,
                       OnInstallFinishedCallback install_callback) override;

  // Called from Java end.
  void OnRequestInstallArModuleResult(JNIEnv* env, bool success);
  void OnRequestInstallSupportedArCoreResult(JNIEnv* env, bool success);

 private:
  // Returns true if AR module installation is supported, false otherwise.
  bool CanRequestInstallArModule();
  // Returns true if AR module is not installed, false otherwise.
  bool ShouldRequestInstallArModule();
  void RequestInstallArModule();
  bool ShouldRequestInstallSupportedArCore();
  void RequestInstallSupportedArCore();

  void RequestArModule();
  void OnRequestArModuleResult(bool success);
  void RequestArCoreInstallOrUpdate();
  void OnRequestArCoreInstallOrUpdateResult(bool success);

  void RunInstallFinishedCallback(bool succeeded);

  base::WeakPtr<ArCoreInstallHelper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  OnInstallFinishedCallback install_finished_callback_;

  base::OnceCallback<void(bool)> on_request_ar_module_result_callback_;
  base::OnceCallback<void(bool)>
      on_request_arcore_install_or_update_result_callback_;

  base::android::ScopedJavaGlobalRef<jobject> jdelegate_;
  int render_process_id_;
  int render_frame_id_;

  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ArCoreInstallHelper> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
