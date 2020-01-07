// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

namespace web_app {

std::string ProviderTypeParamToString(
    const ::testing::TestParamInfo<ProviderType>& provider_type) {
  switch (provider_type.param) {
    case ProviderType::kBookmarkApps:
      return "BookmarkApps";
    case ProviderType::kWebApps:
      return "WebApps";
  }
}

}  // namespace web_app
