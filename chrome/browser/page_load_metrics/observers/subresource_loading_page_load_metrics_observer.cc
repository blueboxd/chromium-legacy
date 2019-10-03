// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_loading_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

bool IsCSSOrJS(const std::string& mime_type) {
  std::string lower_mime_type = base::ToLowerASCII(mime_type);
  return lower_mime_type == "text/css" ||
         blink::IsSupportedJavascriptMimeType(lower_mime_type);
}

}  // namespace

SubresourceLoadingPageLoadMetricsObserver::
    SubresourceLoadingPageLoadMetricsObserver() = default;

SubresourceLoadingPageLoadMetricsObserver::
    ~SubresourceLoadingPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return STOP_OBSERVING;

  if (!navigation_handle->IsInMainFrame())
    return STOP_OBSERVING;

  if (!page_load_metrics::IsNavigationUserInitiated(navigation_handle))
    return STOP_OBSERVING;

  if (!GetDelegate().StartedInForeground())
    return STOP_OBSERVING;

  return CONTINUE_OBSERVING;
}

void SubresourceLoadingPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (const auto& resource : resources) {
    if (!resource->is_complete)
      continue;

    if (!resource->is_main_frame_resource)
      continue;

    if (!IsCSSOrJS(resource->mime_type))
      continue;

    if (!resource->completed_before_fcp)
      continue;

    if (resource->cache_type ==
        page_load_metrics::mojom::CacheType::kNotCached) {
      loaded_css_js_from_network_before_fcp_++;
    } else {
      loaded_css_js_from_cache_before_fcp_++;
    }
  }
}

void SubresourceLoadingPageLoadMetricsObserver::RecordMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/995437): Add UKM for data saver users once all metrics are
  // in place.
  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached",
      loaded_css_js_from_cache_before_fcp_);
  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached",
      loaded_css_js_from_network_before_fcp_);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

void SubresourceLoadingPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
}
