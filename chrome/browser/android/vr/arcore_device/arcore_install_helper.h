// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/xr_install_helper.h"

namespace vr {

class VR_EXPORT ArCoreInstallHelper : public content::XrInstallHelper {
 public:
  ArCoreInstallHelper();
  ~ArCoreInstallHelper() override;

  ArCoreInstallHelper(const ArCoreInstallHelper&) = delete;
  ArCoreInstallHelper& operator=(const ArCoreInstallHelper&) = delete;

  void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> install_callback) override;

  // Called from Java end.
  void OnRequestInstallSupportedArCoreResult(JNIEnv* env, bool success);

 private:
  void RunInstallFinishedCallback(bool succeeded);

  base::OnceCallback<void(bool)> install_finished_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
