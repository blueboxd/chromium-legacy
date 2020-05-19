// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cookie_utils.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

namespace {

void RecordContextDowngradeUKM(
    RenderFrameHost* rfh,
    CookieAccessDetails::Type access_type,
    const net::CanonicalCookie::CookieInclusionStatus& status,
    const GURL& url) {
  DCHECK(rfh);
  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

  if (access_type == CookieAccessDetails::Type::kRead) {
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetRequestPerCookie(status.GetBreakingDowngradeMetricsEnumValue(url))
        .Record(ukm::UkmRecorder::Get());
  } else {
    DCHECK(access_type == CookieAccessDetails::Type::kChange);
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetResponsePerCookie(status.GetBreakingDowngradeMetricsEnumValue(url))
        .Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace

void SplitCookiesIntoAllowedAndBlocked(
    const network::mojom::CookieAccessDetailsPtr& cookie_details,
    CookieAccessDetails* allowed,
    CookieAccessDetails* blocked) {
  *allowed =
      CookieAccessDetails({cookie_details->type,
                           cookie_details->url,
                           cookie_details->site_for_cookies.RepresentativeUrl(),
                           {},
                           /* blocked_by_policy=*/false});
  *blocked =
      CookieAccessDetails({cookie_details->type,
                           cookie_details->url,
                           cookie_details->site_for_cookies.RepresentativeUrl(),
                           {},
                           /* blocked_by_policy=*/true});

  for (auto& cookie_and_status : cookie_details->cookie_list) {
    if (cookie_and_status.status.HasExclusionReason(
            net::CanonicalCookie::CookieInclusionStatus::
                EXCLUDE_USER_PREFERENCES)) {
      blocked->cookie_list.push_back(std::move(cookie_and_status.cookie));
    } else if (cookie_and_status.status.IsInclude()) {
      allowed->cookie_list.push_back(std::move(cookie_and_status.cookie));
    }
  }
}

void EmitSameSiteCookiesDeprecationWarning(
    RenderFrameHostImpl* rfh,
    const network::mojom::CookieAccessDetailsPtr& cookie_details) {
  RenderFrameHostImpl* root_frame_host = rfh->GetMainFrame();

  if (!root_frame_host->IsCurrent())
    return;

  bool samesite_treated_as_lax_cookies = false;
  bool samesite_none_insecure_cookies = false;

  bool messages_disabled_by_cmdline =
      base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kCookieDeprecationMessages.name,
          base::FeatureList::OVERRIDE_DISABLE_FEATURE);

  bool breaking_context_downgrade = false;

  for (const net::CookieWithStatus& excluded_cookie :
       cookie_details->cookie_list) {
    std::string cookie_url =
        net::cookie_util::CookieOriginToURL(excluded_cookie.cookie.Domain(),
                                            excluded_cookie.cookie.IsSecure())
            .possibly_invalid_spec();

    if (excluded_cookie.status.ShouldWarn()) {
      if (excluded_cookie.status.HasWarningReason(
              net::CanonicalCookie::CookieInclusionStatus::
                  WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)) {
        samesite_treated_as_lax_cookies = true;
      }

      if (excluded_cookie.status.HasWarningReason(
              net::CanonicalCookie::CookieInclusionStatus::
                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE)) {
        samesite_treated_as_lax_cookies = true;
      }

      if (excluded_cookie.status.HasWarningReason(
              net::CanonicalCookie::CookieInclusionStatus::
                  WARN_SAMESITE_NONE_INSECURE)) {
        samesite_none_insecure_cookies = true;
      }
      devtools_instrumentation::ReportSameSiteCookieIssue(
          root_frame_host, excluded_cookie, cookie_details->url,
          cookie_details->site_for_cookies,
          cookie_details->type == CookieAccessDetails::Type::kRead
              ? blink::mojom::SameSiteCookieOperation::ReadCookie
              : blink::mojom::SameSiteCookieOperation::SetCookie,
          cookie_details->devtools_request_id);
    }
    if (!messages_disabled_by_cmdline) {
      root_frame_host->AddSameSiteCookieDeprecationMessage(
          cookie_url, excluded_cookie.status,
          net::cookie_util::IsSameSiteByDefaultCookiesEnabled(),
          net::cookie_util::IsCookiesWithoutSameSiteMustBeSecureEnabled());
    }

    breaking_context_downgrade = breaking_context_downgrade ||
                                 excluded_cookie.status.HasDowngradeWarning();

    if (excluded_cookie.status.HasDowngradeWarning()) {
      // Unlike with UMA, do not record cookies that have no downgrade warning.
      RecordContextDowngradeUKM(rfh, cookie_details->type,
                                excluded_cookie.status, cookie_details->url);
    }
  }

  // TODO(crbug.com/990439): Do we need separate UseCounter metrics for
  // Lax-allow-unsafe? We already have histograms in CanonicalCookie.
  if (samesite_treated_as_lax_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCookieNoSameSite);
  }

  if (samesite_none_insecure_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCookieInsecureAndSameSiteNone);
  }

  if (breaking_context_downgrade) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kSchemefulSameSiteContextDowngrade);
  }
}

}  // namespace content
