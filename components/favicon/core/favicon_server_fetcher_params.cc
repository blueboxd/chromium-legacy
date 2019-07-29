// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_server_fetcher_params.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/favicon_base/favicon_util.h"
#include "ui/gfx/favicon_size.h"

namespace favicon {
namespace {

const char kClientParamDesktop[] = "client=chrome_desktop";
const char kClientParamMobile[] = "client=chrome";
// TODO(https://crbug.com/982810): Refactor to remove ios specific code from
// here.
#if defined(OS_IOS)
const int kMobileSizeInDip = 32;
#else
const int kMobileSizeInDip = 24;
#endif

float GetMaxDeviceScaleFactor() {
  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  DCHECK(!favicon_scales.empty());
  return favicon_scales.back();
}

}  // namespace

std::unique_ptr<FaviconServerFetcherParams>
FaviconServerFetcherParams::CreateForDesktop(const GURL& page_url) {
  return base::WrapUnique(new FaviconServerFetcherParams(
      page_url, favicon_base::IconType::kFavicon,
      std::ceil(gfx::kFaviconSize * GetMaxDeviceScaleFactor()),
      kClientParamDesktop));
}

std::unique_ptr<FaviconServerFetcherParams>
FaviconServerFetcherParams::CreateForMobile(const GURL& page_url) {
  return base::WrapUnique(new FaviconServerFetcherParams(
      page_url, favicon_base::IconType::kTouchIcon,
      std::ceil(kMobileSizeInDip * GetMaxDeviceScaleFactor()),
      kClientParamMobile));
}

FaviconServerFetcherParams::FaviconServerFetcherParams(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    int desired_size_in_pixel,
    const std::string& google_server_client_param)
    : page_url_(page_url),
      icon_type_(icon_type),
      desired_size_in_pixel_(desired_size_in_pixel),
      google_server_client_param_(google_server_client_param) {}

FaviconServerFetcherParams::~FaviconServerFetcherParams() = default;

}  // namespace favicon
