// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <stdint.h>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/base32/base32.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"

bool HandlePreviewsLitePageURLRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Don't change the |url|, just register our interest in reversing it before
  // it is displayed to the user in |HandlePreviewsLitePageURLRewriteReverse|.
  // Without returning true here, |HandlePreviewsLitePageURLRewriteReverse|
  // would not be called.

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);

  if (!data_reduction_proxy_settings)
    return false;

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled() &&
         previews::params::IsLitePageServerPreviewsEnabled();
}

bool HandlePreviewsLitePageURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(*url,
                                                          &original_url)) {
    *url = GURL(original_url);
    return true;
  }
  return false;
}

// static
void PreviewsLitePageNavigationThrottle::LogIneligibleReason(
    IneligibleReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.IneligibleReasons",

                            reason);
}

// static
GURL PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
    const GURL& original_url) {
  DCHECK(original_url.is_valid());
  const std::string experiment_id =
      data_reduction_proxy::params::GetDataSaverServerExperiments();

  std::string experiment_query;
  if (!experiment_id.empty()) {
    experiment_query =
        "&x=" + net::EscapeQueryParamValue(experiment_id, true /* use_plus */);
  }
  std::string fragment;
  if (original_url.has_ref()) {
    fragment = "#" + original_url.ref();
  }

  // Strip out the fragment so that it is not sent to the server.
  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::NumberToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  GURL previews_url = GURL(
      previews_host.scheme() + "://" + origin_hash + "." +
      previews_host.host() +
      (previews_host.has_port() ? (":" + previews_host.port()) : "") + "/p?u=" +
      net::EscapeQueryParamValue(original_url.GetAsReferrer().spec(),
                                 true /* use_plus */) +
      experiment_query + fragment);
  DCHECK(previews_url.is_valid());
  DCHECK_EQ(previews_host.scheme(), previews_url.scheme());
  return previews_url;
}

// static
previews::PreviewsUserData::ServerLitePageInfo*
PreviewsLitePageNavigationThrottle::GetOrCreateServerLitePageInfo(
    content::NavigationHandle* navigation_handle,
    PreviewsLitePageNavigationThrottleManager* manager) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return nullptr;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_data)
    return nullptr;

  if (previews_data->server_lite_page_info()) {
    return previews_data->server_lite_page_info();
  }

  previews_data->set_server_lite_page_info(
      std::make_unique<previews::PreviewsUserData::ServerLitePageInfo>());

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  base::Optional<std::string> session_id;
  if (drp_settings) {
    session_id = data_reduction_proxy::DataReductionProxyRequestOptions::
        GetSessionKeyFromRequestHeaders(drp_settings->GetProxyRequestHeaders());
  }

  previews::PreviewsUserData::ServerLitePageInfo* info =
      previews_data->server_lite_page_info();
  info->original_navigation_start = navigation_handle->NavigationStart();
  if (session_id.has_value())
    info->drp_session_key = session_id.value();

  const ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<const ChromeNavigationUIData*>(
          navigation_handle->GetNavigationUIData());
  if (chrome_navigation_ui_data)
    info->page_id = chrome_navigation_ui_data->data_reduction_proxy_page_id();
  // The page id may not be set in some corner cases (like forward navigation),
  // so make sure it gets set here.
  if (info->page_id == 0U)
    info->page_id = manager->GeneratePageID();

  return info;
}
