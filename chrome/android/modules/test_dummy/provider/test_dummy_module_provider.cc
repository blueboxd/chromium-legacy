// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/apk_assets.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/android/modules/test_dummy/provider/jni_headers/TestDummyModuleProvider_jni.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"

// Native partner of the Java TestDummyModuleProvider. Used to load native
// resources of the test dummy module into memory.
// TODO(tiborg): Move to //components/module_installer so that it can be used by
// other modules.

namespace module_installer {

// Allows memory mapping module PAK files on the main thread.
//
// We expect the memory mapping step to be quick. All it does is retrieveing a
// region from the APK file that should already be memory mapped and reading the
// PAK file header. Most of the disk IO is happening when accessing a resource.
// And this traditionally happens synchronously on the main thread.
class ScopedAllowModulePakLoad {
 public:
  ScopedAllowModulePakLoad() {}
  ~ScopedAllowModulePakLoad() {}

 private:
  base::ScopedAllowBlocking allow_blocking_;
};

}  // namespace module_installer

namespace test_dummy {

static void JNI_TestDummyModuleProvider_LoadNative(JNIEnv* env) {
  module_installer::ScopedAllowModulePakLoad resource_loader;
  std::string path = "assets/test_dummy_resources.pak";
  base::MemoryMappedFile::Region region;
  int fd = base::android::OpenApkAsset(path, &region);
  if (fd < 0) {
    NOTREACHED() << "Cannot find " << path << " in APK.";
  }
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(fd), region, ui::SCALE_FACTOR_NONE);
}

}  // namespace test_dummy
