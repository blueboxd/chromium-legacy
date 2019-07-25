// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "components/module_installer/android/module_installer_jni_headers/ModuleInstallerBackend_jni.h"

namespace module_installer {

// Handles native specific initialization in after module is installed. This may
// be blocking, so the call should be done asynchronously.
void JNI_ModuleInstallerBackend_OnInstallModule(
    JNIEnv* env,
    jboolean j_success,
    const base::android::JavaParamRef<jstring>& j_module_name) {
  // TODO(crbug.com/927131): Add code to detect DevUI DFM installation, and load
  // the associated resource PAK file. This will require the following:
  //   bool success = j_success;
  //   (Needs #include "base/android/jni_string.h").
  //   std::string module_name(ConvertJavaStringToUTF8(j_module_name));

  // TODO(crbug.com/986960): Specialize this function to perform resource
  // loading only. Required parameters can be injected to support data-driven
  // flow. This will require massive refactoring first, however.
}

}  // namespace module_installer
