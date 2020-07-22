// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

namespace web_app {

void CreateIconInfoForSystemWebApp(const GURL& app_url,
                                   const std::string& icon_name,
                                   SquareSizePx square_size_px,
                                   int resource_id,
                                   WebApplicationInfo& web_app) {
  web_app.icon_infos.emplace_back(app_url.Resolve(icon_name), square_size_px);
  auto image =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
  web_app.icon_bitmaps_any[square_size_px] = image.AsBitmap();
}

}  // namespace web_app
