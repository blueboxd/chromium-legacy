// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_lite_page_redirect.h"

#include "base/logging.h"
#include "components/previews/core/previews_experiments.h"
#include "net/base/url_util.h"

namespace previews {

// If you're adding values to this switch, also make sure to update
// |Previews.ServerLitePage.Penalty.Types| in histograms.xml.
std::string ServerLitePageStatusToString(ServerLitePageStatus status) {
  switch (status) {
    case ServerLitePageStatus::kUnknown:
      // Unknown penalty metrics are not recorded.
      NOTREACHED();
      return "Unknown";
    case ServerLitePageStatus::kSuccess:
      return "Success";
    case ServerLitePageStatus::kBypass:
      return "Bypass";
    case ServerLitePageStatus::kRedirect:
      return "Redirect";
    case ServerLitePageStatus::kFailure:
      return "Failure";
    case ServerLitePageStatus::kControl:
      // Control group penalty metrics are not recorded.
      NOTREACHED();
      return "Control";
  }
}

bool IsLitePageRedirectPreviewDomain(const GURL& url) {
  if (!url.is_valid())
    return false;

  GURL previews_host = params::GetLitePagePreviewsDomainURL();
  return url.DomainIs(previews_host.host()) &&
         url.EffectiveIntPort() == previews_host.EffectiveIntPort();
}

bool IsLitePageRedirectPreviewURL(const GURL& url) {
  return ExtractOriginalURLFromLitePageRedirectURL(url, nullptr);
}

bool ExtractOriginalURLFromLitePageRedirectURL(const GURL& url,
                                               std::string* original_url) {
  if (!IsLitePageRedirectPreviewDomain(url))
    return false;

  std::string original_url_query_param;
  if (!net::GetValueForKeyInQuery(url, "u", &original_url_query_param))
    return false;

  if (original_url) {
    if (url.has_ref())
      original_url_query_param += "#" + url.ref();
    *original_url = original_url_query_param;
  }
  return true;
}

}  // namespace previews
