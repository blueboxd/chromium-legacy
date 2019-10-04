// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_ui/dev_ui_url_handler.h"

#include <string>

#include "chrome/browser/android/dev_ui/dev_ui_module_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Decides whether WebUI |host| (assumed for chrome://) is in the DevUI DFM.
bool IsWebUiHostInDevUiDfm(const std::string& host) {
  // Each WebUI host (including synonyms) in the DevUI DFM must have an entry.
  // Assume linear search is fast enough. Can optimize later if needed.
  return host == chrome::kChromeUIAccessibilityHost ||
         host == chrome::kChromeUIAutofillInternalsHost ||
         host == chrome::kChromeUIBluetoothInternalsHost ||
         host == chrome::kChromeUIComponentsHost ||
         host == chrome::kChromeUICrashesHost ||
         host == chrome::kChromeUIDeviceLogHost ||
         host == chrome::kChromeUIDomainReliabilityInternalsHost ||
         host == chrome::kChromeUIDownloadInternalsHost ||
         host == chrome::kChromeUIGCMInternalsHost ||
         host == chrome::kChromeUIInterstitialHost ||
         host == chrome::kChromeUIInterventionsInternalsHost ||
         host == chrome::kChromeUIInvalidationsHost ||
         host == chrome::kChromeUILocalStateHost ||
         host == chrome::kChromeUIMediaEngagementHost ||
         host == chrome::kChromeUIMemoryInternalsHost ||
         host == chrome::kChromeUINTPTilesInternalsHost ||
         host == chrome::kChromeUINetExportHost ||
         host == chrome::kChromeUINetInternalsHost ||
         host == chrome::kChromeUINotificationsInternalsHost ||
         host == chrome::kChromeUIOmniboxHost ||
         host == chrome::kChromeUIPasswordManagerInternalsHost ||
         host == chrome::kChromeUIPolicyHost ||
         host == chrome::kChromeUIPredictorsHost ||
         host == chrome::kChromeUIQuotaInternalsHost ||
         host == chrome::kChromeUISandboxHost ||
         host == chrome::kChromeUISignInInternalsHost ||
         host == chrome::kChromeUISiteEngagementHost ||
         host == chrome::kChromeUISnippetsInternalsHost ||
         host == chrome::kChromeUISuggestionsHost ||
         host == chrome::kChromeUISupervisedUserInternalsHost ||
         host == chrome::kChromeUISyncInternalsHost ||
         host == chrome::kChromeUITranslateInternalsHost ||
         host == chrome::kChromeUIUkmHost ||
         host == chrome::kChromeUIUsbInternalsHost ||
         host == chrome::kChromeUIUserActionsHost ||
         host == chrome::kChromeUIWebApksHost ||
         host == chrome::kChromeUIWebRtcLogsHost ||
         host == content::kChromeUIAppCacheInternalsHost ||
         host == content::kChromeUIBlobInternalsHost ||
         host == content::kChromeUIGpuHost ||
         host == content::kChromeUIHistogramHost ||
         host == content::kChromeUIIndexedDBInternalsHost ||
         host == content::kChromeUIMediaInternalsHost ||
         host == content::kChromeUINetworkErrorsListingHost ||
         host == content::kChromeUIProcessInternalsHost ||
         host == content::kChromeUIServiceWorkerInternalsHost ||
         host == content::kChromeUIWebRTCInternalsHost ||
         host == safe_browsing::kChromeUISafeBrowsingHost;
}

}  // namespace

namespace chrome {
namespace android {

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
