// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/module_installer/android/jni_headers/Module_jni.h"

using base::android::BundleUtils;

#if defined(LOAD_FROM_BASE_LIBRARY)
extern "C" {

// These methods are forward-declared here for the build case where
// partitioned libraries are disabled, and module code is pulled into the main
// library. In that case, there is no current way of dlsym()'ing for this
// JNI registration method, and hence it's referred to directly.
// This list could be auto-generated in the future.
extern bool JNI_OnLoad_test_dummy(JNIEnv* env) __attribute__((weak_import));

}  // extern "C"
#endif  // LOAD_FROM_BASE_LIBRARY

namespace module_installer {

typedef bool JniRegistrationFunction(JNIEnv* env);

static void JNI_Module_LoadNativeLibrary(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jtext) {
  std::string library_name;
  base::android::ConvertJavaStringToUTF8(env, jtext, &library_name);

  JniRegistrationFunction* registration_function = nullptr;

#if defined(LOAD_FROM_PARTITIONS) || defined(LOAD_FROM_COMPONENTS)
#if defined(LOAD_FROM_PARTITIONS)
  // The partition library must be opened via native code (using
  // android_dlopen_ext() under the hood). There is no need to repeat the
  // operation on the Java side, because JNI registration is done explicitly
  // (hence there is no reason for the Java ClassLoader to be aware of the
  // library, for lazy JNI registration).
  void* library_handle =
      BundleUtils::DlOpenModuleLibraryPartition(library_name);
#else   // defined(LOAD_FROM_PARTITIONS)
  const std::string lib_name = "lib" + library_name + ".cr.so";
  void* library_handle = dlopen(lib_name.c_str(), RTLD_LOCAL);
#endif  // defined(LOAD_FROM_PARTITIONS)
  CHECK(library_handle != nullptr)
      << "Could not open feature library:" << dlerror();

  const std::string registration_name = "JNI_OnLoad_" + library_name;
  // Find the module's JNI registration method from the feature library.
  void* symbol = dlsym(library_handle, registration_name.c_str());
  CHECK(symbol != nullptr) << "Could not find JNI registration method: "
                           << dlerror();
  registration_function = reinterpret_cast<JniRegistrationFunction*>(symbol);
#else   // defined(LOAD_FROM_PARTITIONS) || defined(LOAD_FROM_COMPONENTS)
  // TODO(https://crbug.com/870055): Similar to the declarations above, this map
  // could be auto-generated.
  const std::map<std::string, JniRegistrationFunction*> modules = {
      {"test_dummy", JNI_OnLoad_test_dummy}};
  registration_function = modules.at(library_name);
#endif  // defined(LOAD_FROM_PARTITIONS) || defined(LOAD_FROM_COMPONENTS)

  CHECK(registration_function(env))
      << "JNI registration failed: " << library_name;
}

}  // namespace module_installer
