// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

bool IsMainFrame(content::RenderFrameHost* rfh) {
  // Don't use rfh->GetRenderViewHost()->GetMainFrame() here because
  // RenderViewHost is being deprecated and because in OOPIF,
  // RenderViewHost::GetMainFrame() returns nullptr for child frames hosted in a
  // different process from the main frame.
  return rfh->GetParent() == nullptr;
}

std::string GetURLWithoutRefParams(const GURL& gurl) {
  url::Replacements<char> replacements;
  replacements.ClearRef();
  return gurl.ReplaceComponents(replacements).spec();
}

// Returns true if |a| and |b| are both valid HTTP/HTTPS URLs and have the
// same scheme, host, path and query params. This method does not take into
// account the ref params of the two URLs.
bool AreGURLsEqualExcludingRefParams(const GURL& a, const GURL& b) {
  return GetURLWithoutRefParams(a) == GetURLWithoutRefParams(b);
}

}  // namespace

struct NavigationPredictor::NavigationScore {
  NavigationScore(const GURL& url,
                  size_t area_rank,
                  double score,
                  bool contains_image)
      : url(url),
        area_rank(area_rank),
        score(score),
        contains_image(contains_image) {}
  // URL of the target link.
  const GURL url;

  // Rank in terms of anchor element area. It starts at 0, a lower rank implies
  // a larger area.
  const size_t area_rank;

  // Calculated navigation score, based on |area_rank| and other metrics.
  double score;

  // Multiple anchor elements may point to the same |url|. |contains_image| is
  // true if at least one of the anchor elements pointing to |url| contains an
  // image.
  const bool contains_image;

  // Rank of the |score| in this document. It starts at 0, a lower rank implies
  // a higher |score|.
  base::Optional<size_t> score_rank;
};

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : browser_context_(
          render_frame_host->GetSiteInstance()->GetBrowserContext()),
      ratio_area_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "ratio_area_scale",
          100)),
      is_in_iframe_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_in_iframe_scale",
          0)),
      is_same_host_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_same_host_scale",
          100)),
      contains_image_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "contains_image_scale",
          50)),
      is_url_incremented_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_url_incremented_scale",
          100)),
      source_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "source_engagement_score_scale",
          0)),
      target_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "target_engagement_score_scale",
          0)),
      area_rank_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "area_rank_scale",
          100)),
      link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "link_total_scale",
          0)),
      iframe_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "iframe_link_total_scale",
          0)),
      increment_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "increment_link_total_scale",
          0)),
      same_origin_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "same_origin_link_total_scale",
          0)),
      image_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "image_link_total_scale",
          0)),
      clickable_space_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "clickable_space_scale",
          0)),
      median_link_location_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "median_link_location_scale",
          0)),
      viewport_height_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "viewport_height_scale",
          0)),
      viewport_width_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "viewport_width_scale",
          0)),
      sum_link_scales_(ratio_area_scale_ + is_in_iframe_scale_ +
                       is_same_host_scale_ + contains_image_scale_ +
                       is_url_incremented_scale_ +
                       source_engagement_score_scale_ +
                       target_engagement_score_scale_ + area_rank_scale_),
      sum_page_scales_(link_total_scale_ + iframe_link_total_scale_ +
                       increment_link_total_scale_ +
                       same_origin_link_total_scale_ + image_link_total_scale_ +
                       clickable_space_scale_ + median_link_location_scale_ +
                       viewport_height_scale_ + viewport_width_scale_),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      prefetch_url_score_threshold_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "prefetch_url_score_threshold",
          0)),
      preconnect_origin_score_threshold_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "preconnect_origin_score_threshold",
          0)),
      same_origin_preconnecting_allowed_(
          base::GetFieldTrialParamByFeatureAsBool(
              blink::features::kNavigationPredictor,
              "same_origin_preconnecting_allowed",
              false)),
      prefetch_after_preconnect_(base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kNavigationPredictor,
          "prefetch_after_preconnect",
          false)),
      normalize_navigation_scores_(base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kNavigationPredictor,
          "normalize_scores",
          true)){
  DCHECK(browser_context_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_LE(0, preconnect_origin_score_threshold_);
  DCHECK_LE(0, prefetch_url_score_threshold_);

  if (!IsMainFrame(render_frame_host))
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  current_visibility_ = web_contents->GetVisibility();
  ukm_source_id_ = web_contents->GetLastCommittedSourceId();
  Observe(web_contents);
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Observe(nullptr);
  if (prerender_handle_) {
    prerender_handle_->OnNavigateAway();
  }
}

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeStrongBinding(
      std::make_unique<NavigationPredictor>(render_frame_host),
      std::move(request));
}

bool NavigationPredictor::IsValidMetricFromRenderer(
    const blink::mojom::AnchorElementMetrics& metric) const {
  return metric.target_url.SchemeIsHTTPOrHTTPS() &&
         metric.source_url.SchemeIsHTTPOrHTTPS();
}


void NavigationPredictor::RecordTimingOnClick() {
  base::TimeTicks current_timing = base::TimeTicks::Now();

  // This is the first click in the document.
  // Note that multiple clicks can happen on the same document. For example,
  // if the click opens a new tab, then the old document is not necessarily
  // destroyed. The user can return to the old document and click.
  if (last_click_timing_ == base::TimeTicks()) {
    // Document may have not loaded yet when click happens.
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.DurationLoadToFirstClick",
                        document_loaded_timing_ > base::TimeTicks()
                            ? current_timing - document_loaded_timing_
                            : base::TimeDelta());
  } else {
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.ClickIntervals",
                        current_timing - last_click_timing_);
  }
  last_click_timing_ = current_timing;
}

void NavigationPredictor::RecordActionAccuracyOnClick(
    const GURL& target_url) const {
  static constexpr char histogram_name_dse[] =
      "NavigationPredictor.OnDSE.AccuracyActionTaken";
  static constexpr char histogram_name_non_dse[] =
      "NavigationPredictor.OnNonDSE.AccuracyActionTaken";

  if (!prefetch_url_ && !preconnect_origin_) {
    base::UmaHistogramEnumeration(source_is_default_search_engine_page_
                                      ? histogram_name_dse
                                      : histogram_name_non_dse,
                                  ActionAccuracy::kNoActionTakenClickHappened);
    return;
  }

  // Exactly one action must have been taken.
  DCHECK(prefetch_url_.has_value() != preconnect_origin_.has_value());

  if (preconnect_origin_) {
    if (url::Origin::Create(target_url) == preconnect_origin_) {
      base::UmaHistogramEnumeration(
          source_is_default_search_engine_page_ ? histogram_name_dse
                                                : histogram_name_non_dse,
          ActionAccuracy::kPreconnectActionClickToSameOrigin);
      return;
    }

    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPreconnectActionClickToDifferentOrigin);
    return;
  }

  DCHECK(prefetch_url_);
  if (target_url == prefetch_url_.value()) {
    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPrefetchActionClickToSameURL);
    return;
  }

  if (url::Origin::Create(target_url) ==
      url::Origin::Create(prefetch_url_.value())) {
    base::UmaHistogramEnumeration(
        source_is_default_search_engine_page_ ? histogram_name_dse
                                              : histogram_name_non_dse,
        ActionAccuracy::kPrefetchActionClickToSameOrigin);
    return;
  }

  base::UmaHistogramEnumeration(
      source_is_default_search_engine_page_ ? histogram_name_dse
                                            : histogram_name_non_dse,
      ActionAccuracy::kPrefetchActionClickToDifferentOrigin);
  return;
}

void NavigationPredictor::OnVisibilityChanged(content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_visibility_ == visibility)
    return;

  // Check if the visibility changed from VISIBLE to HIDDEN. Since navigation
  // predictor is currently restricted to Android, it is okay to disregard the
  // occluded state.
  if (current_visibility_ != content::Visibility::HIDDEN ||
      visibility != content::Visibility::VISIBLE) {
    current_visibility_ = visibility;

    if (prerender_handle_) {
      prerender_handle_->OnNavigateAway();
      prerender_handle_.reset();
    }

    // Stop any future preconnects while hidden.
    timer_.Stop();
    return;
  }

  current_visibility_ = visibility;

  // Previously, the visibility was HIDDEN, and now it is VISIBLE implying that
  // the web contents that was fully hidden is now fully visible.
  MaybePreconnectNow(Action::kPreconnectOnVisibilityChange);
  if (prefetch_url_.has_value()) {
    MaybePrefetch();
  }
}

void NavigationPredictor::MaybePreconnectNow(Action log_action) {
  base::Optional<url::Origin> preconnect_origin = preconnect_origin_;

  if (prefetch_url_ && !preconnect_origin) {
    // Preconnect to the origin of the prefetch URL.
    preconnect_origin = url::Origin::Create(prefetch_url_.value());
  }

  if (!preconnect_origin)
    return;
  if (preconnect_origin->scheme() != url::kHttpScheme &&
      preconnect_origin->scheme() != url::kHttpsScheme) {
    return;
  }

  std::string action_histogram_name =
      source_is_default_search_engine_page_
          ? "NavigationPredictor.OnDSE.ActionTaken"
          : "NavigationPredictor.OnNonDSE.ActionTaken";
  base::UmaHistogramEnumeration(action_histogram_name, log_action);

  if (!same_origin_preconnecting_allowed_)
    return;

  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
  GURL preconnect_url_serialized(preconnect_origin->Serialize());
  DCHECK(preconnect_url_serialized.is_valid());
  loading_predictor->PrepareForPageLoad(
      preconnect_url_serialized, predictors::HintOrigin::NAVIGATION_PREDICTOR,
      true);

  if (current_visibility_ != content::Visibility::VISIBLE)
    return;

  // The delay beyond the idle socket timeout that net uses when
  // re-preconnecting. If negative, no retries occur.
  int retry_delay_ms = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kNavigationPredictor, "retry_preconnect_wait_time_ms",
      50);

  if (retry_delay_ms < 0) {
    return;
  }

  // Set/Reset the timer to fire after the preconnect times out. Add an extra
  // delay to make sure the preconnect has expired if it wasn't used.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          net::features::kNetUnusedIdleSocketTimeout,
          "unused_idle_socket_timeout_seconds", 10)) +
          base::TimeDelta::FromMilliseconds(retry_delay_ms),
      base::BindOnce(&NavigationPredictor::MaybePreconnectNow,
                     base::Unretained(this), Action::kPreconnectAfterTimeout));
}

SiteEngagementService* NavigationPredictor::GetEngagementService() const {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  DCHECK(service);
  return service;
}

TemplateURLService* NavigationPredictor::GetTemplateURLService() const {
  return TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

void NavigationPredictor::ReportAnchorElementMetricsOnClick(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  if (browser_context_->IsOffTheRecord())
    return;

  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics: onClick.");
    return;
  }

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics->source_url);
  if (!metrics->source_url.SchemeIsCryptographic() ||
      !metrics->target_url.SchemeIsCryptographic()) {
    return;
  }

  RecordTimingOnClick();

  SiteEngagementService* engagement_service = GetEngagementService();

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Clicked.DocumentEngagementScore",
      static_cast<int>(engagement_service->GetScore(metrics->source_url)));

  double target_score = engagement_service->GetScore(metrics->target_url);
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.HrefEngagementScore2",
                           static_cast<int>(target_score));
  if (target_score > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScorePositive",
        static_cast<int>(target_score));
  }
  if (!metrics->is_same_host) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScoreExternal",
        static_cast<int>(target_score));
  }

  RecordActionAccuracyOnClick(metrics->target_url);

  // Look up the clicked URL in |navigation_scores_map_|. Record if we find it.
  auto iter = navigation_scores_map_.find(metrics->target_url.spec());
  if (iter == navigation_scores_map_.end())
    return;

  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.AreaRank",
                           static_cast<int>(iter->second->area_rank));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScore",
                           static_cast<int>(iter->second->score));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScoreRank",
                           static_cast<int>(iter->second->score_rank.value()));

  // Guaranteed to be non-zero since we have found the clicked link in
  // |navigation_scores_map_|.
  DCHECK_LT(0, number_of_anchors_);
  if (metrics->is_same_host) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_SameHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_DiffHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors_);
  }

  if (source_is_default_search_engine_page_) {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnDSE.SameHost",
                          metrics->is_same_host);
  } else {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnNonDSE.SameHost",
                          metrics->is_same_host);
  }

  // Check if the clicked anchor element contains image or if any other anchor
  // element pointing to the same url contains an image.
  if (metrics->contains_image || iter->second->contains_image) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_NoImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors_);
  }

  if (metrics->is_in_iframe) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_InIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_NotInIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors_);
  }

  if (metrics->is_url_incremented_by_one) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_UrlIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_NotIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors_);
  }
}

void NavigationPredictor::MergeMetricsSameTargetUrl(
    std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const {
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", metrics->size());

  // Maps from target url (href) to anchor element metrics from renderer.
  std::unordered_map<std::string, blink::mojom::AnchorElementMetricsPtr>
      metrics_map;

  // This size reserve is aggressive since |metrics_map| may contain fewer
  // elements than metrics->size() after merge.
  metrics_map.reserve(metrics->size());

  for (auto& metric : *metrics) {
    // Do not include anchor elements that point to the same URL as the URL of
    // the current navigation since these are unlikely to be clicked. Also,
    // exclude the anchor elements that differ from the URL of the current
    // navigation by only the ref param.
    if (AreGURLsEqualExcludingRefParams(metric->target_url,
                                        metric->source_url)) {
      continue;
    }

    if (!metric->target_url.SchemeIsCryptographic())
      continue;

    // Currently, all predictions are made based on elements that are within the
    // main frame since it is unclear if we can pre* the target of the elements
    // within iframes.
    if (metric->is_in_iframe)
      continue;

    // Skip ref params when merging the anchor elements. This ensures that two
    // anchor elements which differ only in the ref params are combined
    // together.
    const std::string& key = GetURLWithoutRefParams(metric->target_url);
    auto iter = metrics_map.find(key);
    if (iter == metrics_map.end()) {
      metrics_map[key] = std::move(metric);
    } else {
      auto& prev_metric = iter->second;
      prev_metric->ratio_area += metric->ratio_area;
      prev_metric->ratio_visible_area += metric->ratio_visible_area;

      // After merging, value of |ratio_area| can go beyond 1.0. This can
      // happen, e.g., when there are 2 anchor elements pointing to the same
      // target. The first anchor element occupies 90% of the viewport. The
      // second one has size 0.8 times the viewport, and only part of it is
      // visible in the viewport. In that case, |ratio_area| may be 1.7.
      if (prev_metric->ratio_area > 1.0)
        prev_metric->ratio_area = 1.0;
      DCHECK_LE(0.0, prev_metric->ratio_area);
      DCHECK_GE(1.0, prev_metric->ratio_area);

      DCHECK_GE(1.0, prev_metric->ratio_visible_area);

      // Position related metrics are tricky to merge. Another possible way to
      // merge is simply add up the calculated navigation scores.
      prev_metric->ratio_distance_root_top =
          std::min(prev_metric->ratio_distance_root_top,
                   metric->ratio_distance_root_top);
      prev_metric->ratio_distance_root_bottom =
          std::max(prev_metric->ratio_distance_root_bottom,
                   metric->ratio_distance_root_bottom);
      prev_metric->ratio_distance_top_to_visible_top =
          std::min(prev_metric->ratio_distance_top_to_visible_top,
                   metric->ratio_distance_top_to_visible_top);
      prev_metric->ratio_distance_center_to_visible_top =
          std::min(prev_metric->ratio_distance_center_to_visible_top,
                   metric->ratio_distance_center_to_visible_top);

      // Anchor element is not considered in an iframe as long as at least one
      // of them is not in an iframe.
      prev_metric->is_in_iframe =
          prev_metric->is_in_iframe && metric->is_in_iframe;
      prev_metric->contains_image =
          prev_metric->contains_image || metric->contains_image;
      DCHECK_EQ(prev_metric->is_same_host, metric->is_same_host);
    }
  }

  metrics->clear();

  if (metrics_map.empty())
    return;

  metrics->reserve(metrics_map.size());
  for (auto& metric_mapping : metrics_map) {
    metrics->push_back(std::move(metric_mapping.second));
  }

  DCHECK(!metrics->empty());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge",
      metrics->size());
}

void NavigationPredictor::ReportAnchorElementMetricsOnLoad(
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics,
    const gfx::Size& viewport_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  // Each document should only report metrics once when page is loaded.
  DCHECK(navigation_scores_map_.empty());

  if (browser_context_->IsOffTheRecord())
    return;

  if (metrics.empty()) {
    mojo::ReportBadMessage("Bad anchor element metrics: empty.");
    return;
  }

  for (const auto& metric : metrics) {
    if (!IsValidMetricFromRenderer(*metric)) {
      mojo::ReportBadMessage("Bad anchor element metrics: onLoad.");
      return;
    }
  }

  if (!metrics[0]->source_url.SchemeIsCryptographic())
    return;

  document_loaded_timing_ = base::TimeTicks::Now();

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics[0]->source_url);
  MergeMetricsSameTargetUrl(&metrics);

  if (metrics.empty())
    return;

  number_of_anchors_ = metrics.size();
  viewport_size_ = viewport_size;

  // Count the number of anchors that have specific metrics.
  std::vector<double> link_locations;
  link_locations.reserve(metrics.size());

  for (const auto& metric : metrics) {
    number_of_anchors_same_host_ += static_cast<int>(metric->is_same_host);
    number_of_anchors_contains_image_ +=
        static_cast<int>(metric->contains_image);
    number_of_anchors_in_iframe_ += static_cast<int>(metric->is_in_iframe);
    number_of_anchors_url_incremented_ +=
        static_cast<int>(metric->is_url_incremented_by_one);

    link_locations.push_back(metric->ratio_distance_top_to_visible_top);
    total_clickable_space_ += metric->ratio_visible_area;
  }

  sort(link_locations.begin(), link_locations.end());
  median_link_location_ = link_locations[link_locations.size() / 2];
  double page_metrics_score = GetPageMetricsScore();

  // Retrieve site engagement score of the document. |metrics| is guaranteed to
  // be non-empty. All |metrics| have the same source_url.
  SiteEngagementService* engagement_service = GetEngagementService();
  double document_engagement_score =
      engagement_service->GetScore(metrics[0]->source_url);
  DCHECK(document_engagement_score >= 0 &&
         document_engagement_score <= engagement_service->GetMaxPoints());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.DocumentEngagementScore",
      static_cast<int>(document_engagement_score));

  // Sort metric by area in descending order to get area rank, which is a
  // derived feature to calculate navigation score.
  std::sort(metrics.begin(), metrics.end(), [](const auto& a, const auto& b) {
    return a->ratio_area > b->ratio_area;
  });

  // Loop |metrics| to compute navigation scores.
  std::vector<std::unique_ptr<NavigationScore>> navigation_scores;
  navigation_scores.reserve(metrics.size());
  double total_score = 0.0;
  for (size_t i = 0; i != metrics.size(); ++i) {
    const auto& metric = metrics[i];
    RecordMetricsOnLoad(*metric);

    const double target_engagement_score =
        engagement_service->GetScore(metric->target_url);
    DCHECK(target_engagement_score >= 0 &&
           target_engagement_score <= engagement_service->GetMaxPoints());
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Visible.HrefEngagementScore2",
        static_cast<int>(target_engagement_score));
    if (!metric->is_same_host) {
      UMA_HISTOGRAM_COUNTS_100(
          "AnchorElementMetrics.Visible.HrefEngagementScoreExternal",
          static_cast<int>(target_engagement_score));
    }

    // Anchor elements with the same area are assigned with the same rank.
    size_t area_rank = i;
    if (i > 0 && metric->ratio_area == metrics[i - 1]->ratio_area)
      area_rank = navigation_scores[navigation_scores.size() - 1]->area_rank;

    double score =
        CalculateAnchorNavigationScore(*metric, document_engagement_score,
                                       target_engagement_score, area_rank) +
        page_metrics_score;
    total_score += score;

    navigation_scores.push_back(std::make_unique<NavigationScore>(
        metric->target_url, area_rank, score, metric->contains_image));
  }

  if (normalize_navigation_scores_) {
    // Normalize |score| to a total sum of 100.0 across all anchor elements
    // received.
    if (total_score > 0.0) {
      for (auto& navigation_score : navigation_scores) {
        navigation_score->score = navigation_score->score / total_score * 100.0;
      }
    }
  }

  // Sort scores by the calculated navigation score in descending order. This
  // score rank is used by MaybeTakeActionOnLoad, and stored in
  // |navigation_scores_map_|.
  std::sort(navigation_scores.begin(), navigation_scores.end(),
            [](const auto& a, const auto& b) { return a->score > b->score; });

  const url::Origin document_origin =
      url::Origin::Create(metrics[0]->source_url);
  MaybeTakeActionOnLoad(document_origin, navigation_scores);

  // Store navigation scores in |navigation_scores_map_| for fast look up upon
  // clicks.
  navigation_scores_map_.reserve(navigation_scores.size());
  for (size_t i = 0; i != navigation_scores.size(); ++i) {
    navigation_scores[i]->score_rank = base::make_optional(i);
    navigation_scores_map_[navigation_scores[i]->url.spec()] =
        std::move(navigation_scores[i]);
  }
}

double NavigationPredictor::CalculateAnchorNavigationScore(
    const blink::mojom::AnchorElementMetrics& metrics,
    double document_engagement_score,
    double target_engagement_score,
    int area_rank) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  if (sum_link_scales_ == 0)
    return 0.0;

  double max_engagement_points = GetEngagementService()->GetMaxPoints();
  document_engagement_score /= max_engagement_points;
  target_engagement_score /= max_engagement_points;

  double area_rank_score =
      (double)((number_of_anchors_ - area_rank)) / number_of_anchors_;

  DCHECK_LE(0, metrics.ratio_visible_area);
  DCHECK_GE(1, metrics.ratio_visible_area);

  DCHECK_LE(0, metrics.is_in_iframe);
  DCHECK_GE(1, metrics.is_in_iframe);

  DCHECK_LE(0, metrics.is_same_host);
  DCHECK_GE(1, metrics.is_same_host);

  DCHECK_LE(0, metrics.contains_image);
  DCHECK_GE(1, metrics.contains_image);

  DCHECK_LE(0, metrics.is_url_incremented_by_one);
  DCHECK_GE(1, metrics.is_url_incremented_by_one);

  DCHECK_LE(0, document_engagement_score);
  DCHECK_GE(1, document_engagement_score);

  DCHECK_LE(0, target_engagement_score);
  DCHECK_GE(1, target_engagement_score);

  DCHECK_LE(0, area_rank_score);
  DCHECK_GE(1, area_rank_score);

  double host_score = 0.0;
  // On pages from default search engine, give higher weight to target URLs that
  // link to a different host. On non-default search engine pages, give higher
  // weight to target URLs that link to the same host.
  if (!source_is_default_search_engine_page_ && metrics.is_same_host) {
    host_score = is_same_host_scale_;
  } else if (source_is_default_search_engine_page_ && !metrics.is_same_host) {
    host_score = is_same_host_scale_;
  }

  // TODO(chelu): https://crbug.com/850624/. Experiment with other heuristic
  // algorithms for computing the anchor elements score.
  double score = ratio_area_scale_ * metrics.ratio_visible_area +
                 is_in_iframe_scale_ * metrics.is_in_iframe +
                 contains_image_scale_ * metrics.contains_image + host_score +
                 is_url_incremented_scale_ * metrics.is_url_incremented_by_one +
                 source_engagement_score_scale_ * document_engagement_score +
                 target_engagement_score_scale_ * target_engagement_score +
                 area_rank_scale_ * (area_rank_score);

  if (normalize_navigation_scores_) {
    score = score / sum_link_scales_ * 100.0;
    DCHECK_LE(0.0, score);
  }

  return score;
}

double NavigationPredictor::GetPageMetricsScore() const {
  if (sum_page_scales_ == 0.0) {
    return 0;
  } else {
    DCHECK(!viewport_size_.IsEmpty());
    return link_total_scale_ * number_of_anchors_ +
           iframe_link_total_scale_ * number_of_anchors_in_iframe_ +
           increment_link_total_scale_ * number_of_anchors_url_incremented_ +
           same_origin_link_total_scale_ * number_of_anchors_same_host_ +
           image_link_total_scale_ * number_of_anchors_contains_image_ +
           clickable_space_scale_ * total_clickable_space_ +
           median_link_location_scale_ * median_link_location_ +
           viewport_width_scale_ * viewport_size_.width() +
           viewport_height_scale_ * viewport_size_.height();
  }
}

void NavigationPredictor::MaybeTakeActionOnLoad(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) {
  DCHECK(!browser_context_->IsOffTheRecord());

  // |sorted_navigation_scores| are sorted in descending order, the first one
  // has the highest navigation score.
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.HighestNavigationScore",
      static_cast<int>(sorted_navigation_scores[0]->score));

  std::string action_histogram_name =
      source_is_default_search_engine_page_
          ? "NavigationPredictor.OnDSE.ActionTaken"
          : "NavigationPredictor.OnNonDSE.ActionTaken";

  DCHECK(!preconnect_origin_.has_value());
  DCHECK(!prefetch_url_.has_value());

  // Try prefetch first.
  prefetch_url_ = GetUrlToPrefetch(document_origin, sorted_navigation_scores);
  if (prefetch_url_.has_value()) {
    DCHECK_EQ(document_origin.host(), prefetch_url_->host());
    MaybePreconnectNow(Action::kPrefetch);
    MaybePrefetch();
    return;
  }

  // Compute preconnect origin only if there is no valid prefetch URL.
  preconnect_origin_ =
      GetOriginToPreconnect(document_origin, sorted_navigation_scores);
  if (preconnect_origin_.has_value()) {
    DCHECK_EQ(document_origin.host(), preconnect_origin_->host());
    MaybePreconnectNow(Action::kPreconnect);
    return;
  }

  base::UmaHistogramEnumeration(action_histogram_name, Action::kNone);
}

void NavigationPredictor::MaybePrefetch() {
  // If prefetches aren't allowed here, this URL has already
  // been prefetched, or the current tab is hidden,
  // we shouldn't prefetch again.
  if (!prefetch_after_preconnect_ || prefetch_url_prefetched_ ||
      current_visibility_ == content::Visibility::HIDDEN) {
    return;
  }

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          browser_context_);

  if (prerender_manager) {
    Prefetch(prerender_manager);
    prefetch_url_prefetched_ = true;
  }
}

void NavigationPredictor::Prefetch(
    prerender::PrerenderManager* prerender_manager) {
  DCHECK(!prefetch_url_prefetched_);
  DCHECK(!prerender_handle_);

  content::SessionStorageNamespace* session_storage_namespace =
      web_contents()->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size size = web_contents()->GetContainerBounds().size();

  prerender_handle_ = prerender_manager->AddPrerenderFromNavigationPredictor(
      prefetch_url_.value(), session_storage_namespace, size);
}

base::Optional<GURL> NavigationPredictor::GetUrlToPrefetch(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) const {
  // Currently, prefetch is disabled on low-end devices since prefetch may
  // increase memory usage.
  if (is_low_end_device_)
    return base::nullopt;

  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the prefetch is only allowed for same orgins. Hence,
  // prefetch is currently disabled on search engine results page.
  if (source_is_default_search_engine_page_)
    return base::nullopt;

  if (sorted_navigation_scores.empty())
    return base::nullopt;

  // Only the same origin URLs are eligible for prefetching. If the URL with
  // the highest score is from a different origin, then we skip prefetching
  // since same origin URLs are not likely to be clicked.
  if (url::Origin::Create(sorted_navigation_scores[0]->url) !=
      document_origin) {
    return base::nullopt;
  }

  // If the prediction score of the highest scoring URL is less than the
  // threshold, then return.
  if (sorted_navigation_scores[0]->score < prefetch_url_score_threshold_)
    return base::nullopt;

  return sorted_navigation_scores[0]->url;
}

base::Optional<url::Origin> NavigationPredictor::GetOriginToPreconnect(
    const url::Origin& document_origin,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) const {
  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the preconnect is only allowed for same origins. Hence,
  // preconnect is currently disabled on search engine results page.
  if (source_is_default_search_engine_page_)
    return base::nullopt;

  if (base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kNavigationPredictor, "preconnect_skip_link_scores",
          false)) {
    return document_origin;
  }

  // Compute preconnect score for each origins: Multiple anchor elements on
  // the webpage may point to the same origin. The preconnect score for an
  // origin is computed by taking sum of score of all anchor elements that
  // point to that origin.
  std::map<url::Origin, double> preconnect_score_by_origin_map;
  for (const auto& navigation_score : sorted_navigation_scores) {
    const url::Origin origin = url::Origin::Create(navigation_score->url);

    auto iter = preconnect_score_by_origin_map.find(origin);
    if (iter == preconnect_score_by_origin_map.end()) {
      preconnect_score_by_origin_map[origin] = navigation_score->score;
    } else {
      double& existing_metric = iter->second;
      existing_metric += navigation_score->score;
    }
  }

  struct ScoreByOrigin {
    url::Origin origin;
    double score;

    ScoreByOrigin(const url::Origin& origin, double score)
        : origin(origin), score(score) {}
  };

  // |sorted_preconnect_scores| would contain preconnect scores of different
  // origins sorted in descending order of the preconnect score.
  std::vector<ScoreByOrigin> sorted_preconnect_scores;

  // First copy all entries from |preconnect_score_by_origin_map| to
  // |sorted_preconnect_scores|.
  for (const auto& score_by_origin_map_entry : preconnect_score_by_origin_map) {
    ScoreByOrigin entry(score_by_origin_map_entry.first,
                        score_by_origin_map_entry.second);
    sorted_preconnect_scores.push_back(entry);
  }

  if (sorted_preconnect_scores.empty())
    return base::nullopt;

  // Sort scores by the calculated preconnect score in descending order.
  std::sort(sorted_preconnect_scores.begin(), sorted_preconnect_scores.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });

#if DCHECK_IS_ON()
  // |sum_of_scores| must be close to the total score of 100.
  double sum_of_scores = 0.0;
  for (const auto& score_by_origin : sorted_preconnect_scores)
    sum_of_scores += score_by_origin.score;
  // Allow an error of 2.0. i.e., |sum_of_scores| is expected to be between 98
  // and 102.
  DCHECK_GE(2.0, std::abs(sum_of_scores - 100));
#endif

  // Connect to the origin with highest score provided the origin is same
  // as the document origin.
  if (sorted_preconnect_scores[0].origin != document_origin)
    return base::nullopt;

  // If the prediction score of the highest scoring origin is less than the
  // threshold, then return.
  if (sorted_preconnect_scores[0].score < preconnect_origin_score_threshold_) {
    return base::nullopt;
  }

  return sorted_preconnect_scores[0].origin;
}

void NavigationPredictor::RecordMetricsOnLoad(
    const blink::mojom::AnchorElementMetrics& metric) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioArea",
                           static_cast<int>(metric.ratio_area * 100));

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioVisibleArea",
                           static_cast<int>(metric.ratio_visible_area * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceTopToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_top_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceCenterToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_center_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootTop",
      static_cast<int>(std::min(metric.ratio_distance_root_top, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootBottom",
      static_cast<int>(std::min(metric.ratio_distance_root_bottom, 100.0f) *
                       100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsInIFrame",
                        metric.is_in_iframe);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.ContainsImage",
                        metric.contains_image);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsSameHost",
                        metric.is_same_host);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsUrlIncrementedByOne",
                        metric.is_url_incremented_by_one);
}
