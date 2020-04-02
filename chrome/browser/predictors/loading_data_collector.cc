// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/browser_features.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_stats_collector.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

bool g_allow_port_in_urls = true;

// Sorted by decreasing likelihood according to HTTP archive.
const char* const kFontMimeTypes[] = {"font/woff2",
                                      "application/x-font-woff",
                                      "application/font-woff",
                                      "application/font-woff2",
                                      "font/x-woff",
                                      "application/x-font-ttf",
                                      "font/woff",
                                      "font/ttf",
                                      "application/x-font-otf",
                                      "x-font/woff",
                                      "application/font-sfnt",
                                      "application/font-ttf"};

// Determines the request destination from the mime type, defaulting to the
// |fallback| if the destination could not be determined.
network::mojom::RequestDestination GetRequestDestinationFromMimeType(
    const std::string& mime_type,
    network::mojom::RequestDestination fallback) {
  if (mime_type.empty()) {
    return fallback;
  } else if (blink::IsSupportedImageMimeType(mime_type)) {
    return network::mojom::RequestDestination::kImage;
  } else if (blink::IsSupportedJavascriptMimeType(mime_type)) {
    return network::mojom::RequestDestination::kScript;
  } else if (net::MatchesMimeType("text/css", mime_type)) {
    return network::mojom::RequestDestination::kStyle;
  } else {
    bool found =
        std::any_of(std::begin(kFontMimeTypes), std::end(kFontMimeTypes),
                    [&mime_type](const std::string& mime) {
                      return net::MatchesMimeType(mime, mime_type);
                    });
    if (found)
      return network::mojom::RequestDestination::kFont;
  }
  return fallback;
}

// Determines the resource type from the declared one, falling back to MIME
// type detection when it is not explicit.
network::mojom::RequestDestination GetRequestDestination(
    network::mojom::RequestDestination destination,
    const std::string& mime_type) {
  // Restricts empty request destination (e.g. prefetch, prerender, fetch,
  // xhr etc) to a small set of mime types, because these destination types
  // don't communicate how the resources will be used.
  if (destination == network::mojom::RequestDestination::kEmpty) {
    return GetRequestDestinationFromMimeType(
        mime_type, network::mojom::RequestDestination::kEmpty);
  }
  return destination;
}

}  // namespace

OriginRequestSummary::OriginRequestSummary() = default;
OriginRequestSummary::OriginRequestSummary(const OriginRequestSummary& other) =
    default;
OriginRequestSummary::~OriginRequestSummary() = default;

PageRequestSummary::PageRequestSummary(const NavigationID& navigation_id)
    : ukm_source_id(navigation_id.ukm_source_id),
      main_frame_url(navigation_id.main_frame_url),
      initial_url(navigation_id.main_frame_url),
      first_contentful_paint(base::TimeTicks::Max()) {}

PageRequestSummary::PageRequestSummary(const PageRequestSummary& other) =
    default;

void PageRequestSummary::UpdateOrAddResource(
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  for (const auto& redirect_info : resource_load_info.redirect_info_chain) {
    UpdateOrAddToOrigins(redirect_info->origin_of_new_url,
                         redirect_info->network_info);
  }
  UpdateOrAddToOrigins(url::Origin::Create(resource_load_info.final_url),
                       resource_load_info.network_info);
  subresource_urls.insert(resource_load_info.final_url);
}

void PageRequestSummary::UpdateOrAddToOrigins(
    const url::Origin& origin,
    const blink::mojom::CommonNetworkInfoPtr& network_info) {
  if (origin.opaque())
    return;

  auto it = origins.find(origin);
  if (it == origins.end()) {
    OriginRequestSummary summary;
    summary.origin = origin;
    summary.first_occurrence = origins.size();
    it = origins.insert({origin, summary}).first;
  }

  it->second.always_access_network |= network_info->always_access_network;
  it->second.accessed_network |= network_info->network_accessed;
}

PageRequestSummary::~PageRequestSummary() = default;

// static
void LoadingDataCollector::SetAllowPortInUrlsForTesting(bool state) {
  g_allow_port_in_urls = state;
}

LoadingDataCollector::LoadingDataCollector(
    ResourcePrefetchPredictor* predictor,
    predictors::LoadingStatsCollector* stats_collector,
    const LoadingPredictorConfig& config)
    : predictor_(predictor),
      stats_collector_(stats_collector),
      config_(config) {}

LoadingDataCollector::~LoadingDataCollector() = default;

void LoadingDataCollector::RecordStartNavigation(
    const NavigationID& navigation_id) {
  CleanupAbandonedNavigations(navigation_id);

  // New empty navigation entry.
  inflight_navigations_.emplace(
      navigation_id, std::make_unique<PageRequestSummary>(navigation_id));
}

void LoadingDataCollector::RecordFinishNavigation(
    const NavigationID& old_navigation_id,
    const NavigationID& new_navigation_id,
    bool is_error_page) {
  if (is_error_page) {
    inflight_navigations_.erase(old_navigation_id);
    return;
  }

  // All subsequent events corresponding to this navigation will have
  // |new_navigation_id|. Find the |old_navigation_id| entry in
  // |inflight_navigations_| and change its key to the |new_navigation_id|.
  std::unique_ptr<PageRequestSummary> summary;
  auto nav_it = inflight_navigations_.find(old_navigation_id);
  if (nav_it != inflight_navigations_.end()) {
    summary = std::move(nav_it->second);
    DCHECK_EQ(summary->main_frame_url, old_navigation_id.main_frame_url);
    summary->main_frame_url = new_navigation_id.main_frame_url;
    inflight_navigations_.erase(nav_it);
  } else {
    summary = std::make_unique<PageRequestSummary>(new_navigation_id);
    summary->initial_url = old_navigation_id.main_frame_url;
  }

  inflight_navigations_.emplace(new_navigation_id, std::move(summary));
}

void LoadingDataCollector::RecordResourceLoadComplete(
    const NavigationID& navigation_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  if (!ShouldRecordResourceLoad(navigation_id, resource_load_info))
    return;

  auto& page_request_summary = *nav_it->second;
  page_request_summary.UpdateOrAddResource(resource_load_info);
}

void LoadingDataCollector::RecordMainFrameLoadComplete(
    const NavigationID& navigation_id,
    const base::Optional<OptimizationGuidePrediction>&
        optimization_guide_prediction) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Initialize |predictor_| no matter whether the |navigation_id| is present in
  // |inflight_navigations_|. This is the case for NTP and about:blank pages,
  // for example.
  if (predictor_)
    predictor_->StartInitialization();

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  // Remove the navigation from the inflight navigations.
  std::unique_ptr<PageRequestSummary> summary = std::move(nav_it->second);
  inflight_navigations_.erase(nav_it);

  if (stats_collector_) {
    stats_collector_->RecordPageRequestSummary(*summary,
                                               optimization_guide_prediction);
  }

  if (predictor_)
    predictor_->RecordPageRequestSummary(std::move(summary));
}

void LoadingDataCollector::RecordFirstContentfulPaint(
    const NavigationID& navigation_id,
    const base::TimeTicks& first_contentful_paint) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it != inflight_navigations_.end())
    nav_it->second->first_contentful_paint = first_contentful_paint;
}

bool LoadingDataCollector::ShouldRecordResourceLoad(
    const NavigationID& navigation_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) const {
  const GURL& url = resource_load_info.final_url;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  if (!g_allow_port_in_urls && url.has_port())
    return false;

  // Guard behind feature: All delayable requests are considered low priority.
  if (base::FeatureList::IsEnabled(
          features::kLoadingOnlyLearnHighPriorityResources) &&
      resource_load_info.request_priority < net::MEDIUM) {
    return false;
  }

  if (!IsHandledResourceType(resource_load_info.request_destination,
                             resource_load_info.mime_type)) {
    return false;
  }
  if (resource_load_info.method != "GET")
    return false;

  return true;
}

// static
bool LoadingDataCollector::IsHandledResourceType(
    network::mojom::RequestDestination destination,
    const std::string& mime_type) {
  network::mojom::RequestDestination actual_destination =
      GetRequestDestination(destination, mime_type);
  return actual_destination == network::mojom::RequestDestination::kDocument ||
         actual_destination == network::mojom::RequestDestination::kStyle ||
         actual_destination == network::mojom::RequestDestination::kScript ||
         actual_destination == network::mojom::RequestDestination::kImage ||
         actual_destination == network::mojom::RequestDestination::kFont;
}

void LoadingDataCollector::CleanupAbandonedNavigations(
    const NavigationID& navigation_id) {
  if (stats_collector_)
    stats_collector_->CleanupAbandonedStats();

  static const base::TimeDelta max_navigation_age =
      base::TimeDelta::FromSeconds(config_.max_navigation_lifetime_seconds);

  base::TimeTicks time_now = base::TimeTicks::Now();
  for (auto it = inflight_navigations_.begin();
       it != inflight_navigations_.end();) {
    if ((it->first.tab_id == navigation_id.tab_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      inflight_navigations_.erase(it++);
    } else {
      ++it;
    }
  }
}

}  // namespace predictors
