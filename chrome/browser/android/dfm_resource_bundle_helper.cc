// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dfm_resource_bundle_helper.h"

#include "base/logging.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "ui/base/resource/resource_bundle_android.h"

namespace android {

#if BUILDFLAG(DFMIFY_DEV_UI)
void LoadDevUiResources() {
  base::FilePath path_on_disk;
  base::PathService::Get(chrome::FILE_DEV_UI_RESOURCES_PACK, &path_on_disk);
  ui::LoadAndroidDevUiPackFile("assets/dev_ui_resources.pak", path_on_disk);
}
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

}  // namespace android
