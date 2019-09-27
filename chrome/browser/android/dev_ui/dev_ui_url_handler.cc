// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_ui/dev_ui_url_handler.h"

#include <string>

#include "chrome/browser/android/dev_ui/dev_ui_module_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace chrome {
namespace android {

namespace {

// Decides whether WebUI |host| (assumed for chrome://) is in the DevUI DFM.
bool IsWebUiHostInDevUiDfm(const std::string& host) {
  // Each WebUI host in the DevUI DFM must have an entry here.
  return host == kChromeUIBluetoothInternalsHost ||
         host == kChromeUIUsbInternalsHost;
}

}  // namespace

bool HandleDfmifiedDevUiPageURL(
    GURL* url,
    content::BrowserContext* /* browser_context */) {
  if (!url->SchemeIs(content::kChromeUIScheme) ||
      !IsWebUiHostInDevUiDfm(url->host()) ||
      dev_ui::DevUiModuleProvider::GetInstance().ModuleLoaded()) {
    // No need to check ModuleInstalled(): It's implied by ModuleLoaded().
    return false;
  }
  // Create URL to the DevUI loader with "?url=<escaped original URL>" so that
  // after install / load, the loader can redirect to the original URL.
  *url = net::AppendQueryParameter(
      GURL(std::string(kChromeUIDevUiLoaderURL) + "dev_ui_loader.html"), "url",
      url->spec());
  return true;
}

}  // namespace android
}  // namespace chrome
