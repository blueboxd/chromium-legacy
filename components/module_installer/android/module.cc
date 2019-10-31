// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <string>

#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "components/module_installer/android/jni_headers/Module_jni.h"
#include "ui/base/resource/resource_bundle_android.h"

using base::android::BundleUtils;

namespace module_installer {

// Allows memory mapping module PAK files on the main thread.
//
// We expect the memory mapping step to be quick. All it does is retrieve a
// region from the APK file that should already be memory mapped and read the
// PAK file header. Most of the disk IO is happening when accessing a resource.
// And this traditionally happens synchronously on the main thread.
//
// This class needs to be a friend of base::ScopedAllowBlocking and so cannot be
// in the unnamed namespace.
class ScopedAllowModulePakLoad {
 public:
  ScopedAllowModulePakLoad() {}
  ~ScopedAllowModulePakLoad() {}

 private:
  base::ScopedAllowBlocking allow_blocking_;
};

namespace {

typedef bool JniRegistrationFunction(JNIEnv* env);

void LoadLibrary(JNIEnv* env, const std::string& library_name) {
  JniRegistrationFunction* registration_function = nullptr;

#if defined(LOAD_FROM_PARTITIONS)
  // The partition library must be opened via native code (using
  // android_dlopen_ext() under the hood). There is no need to repeat the
  // operation on the Java side, because JNI registration is done explicitly
  // (hence there is no reason for the Java ClassLoader to be aware of the
  // library, for lazy JNI registration).
  void* library_handle =
      BundleUtils::DlOpenModuleLibraryPartition(library_name);
#elif defined(COMPONENT_BUILD)
  const std::string lib_name = "lib" + library_name + ".cr.so";
  void* library_handle = dlopen(lib_name.c_str(), RTLD_LOCAL);
#else
#error "Unsupported configuration."
#endif  // defined(COMPONENT_BUILD)
  CHECK(library_handle != nullptr)
      << "Could not open feature library:" << dlerror();

  const std::string registration_name = "JNI_OnLoad_" + library_name;
  // Find the module's JNI registration method from the feature library.
  void* symbol = dlsym(library_handle, registration_name.c_str());
  CHECK(symbol != nullptr) << "Could not find JNI registration method: "
                           << dlerror();
  registration_function = reinterpret_cast<JniRegistrationFunction*>(symbol);
  CHECK(registration_function(env))
      << "JNI registration failed: " << library_name;
}

void LoadResources(const std::string& name) {
  module_installer::ScopedAllowModulePakLoad scoped_allow_module_pak_load;
  ui::LoadPackFileFromApk("assets/" + name + "_resources.pak");
}

}  // namespace

static void JNI_Module_LoadNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jname,
    jboolean load_library,
    jboolean load_resources) {
  std::string name;
  base::android::ConvertJavaStringToUTF8(env, jname, &name);
  if (load_library) {
    LoadLibrary(env, name);
  }
  if (load_resources) {
    LoadResources(name);
  }
}

}  // namespace module_installer
