// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include <cmath>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/protocol_util.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "media/base/mime_util.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/events/blink/blink_features.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif

namespace {

const char kOfflinePreviewsMimeType[] = "multipart/related";

bool IsSupportedProtocol(page_load_metrics::NetworkProtocol protocol) {
  switch (protocol) {
    case page_load_metrics::NetworkProtocol::kHttp11:
      return true;
    case page_load_metrics::NetworkProtocol::kHttp2:
      return true;
    case page_load_metrics::NetworkProtocol::kQuic:
      return true;
    case page_load_metrics::NetworkProtocol::kOther:
      return false;
  }
}

int64_t LayoutShiftUkmValue(float shift_score) {
  // Report (shift_score * 100) as an int in the range [0, 1000].
  return static_cast<int>(roundf(std::min(shift_score, 10.0f) * 100.0f));
}

int32_t LayoutShiftUmaValue(float shift_score) {
  // Report (shift_score * 10) as an int in the range [0, 100].
  return static_cast<int>(roundf(std::min(shift_score, 10.0f) * 10.0f));
}

bool IsDefaultSearchEngine(content::BrowserContext* browser_context,
                           const GURL& url) {
  if (!browser_context)
    return false;

  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));

  if (!template_service)
    return false;

  return template_service->IsSearchResultsPageFromDefaultSearchProvider(url);
}

bool IsUserHomePage(content::BrowserContext* browser_context, const GURL& url) {
  if (!browser_context)
    return false;

  return url.spec() == Profile::FromBrowserContext(browser_context)
                           ->GetPrefs()
                           ->GetString(prefs::kHomePage);
}

}  // namespace

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded() {
  if (!ukm::UkmRecorder::Get()) {
    return nullptr;
  }
  return std::make_unique<UkmPageLoadMetricsObserver>(
      g_browser_process->network_quality_tracker());
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver(
    network::NetworkQualityTracker* network_quality_tracker)
    : network_quality_tracker_(network_quality_tracker),
      largest_contentful_paint_handler_() {
  DCHECK(network_quality_tracker_);
}

UkmPageLoadMetricsObserver::~UkmPageLoadMetricsObserver() = default;

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();

  start_url_is_default_search_ =
      IsDefaultSearchEngine(browser_context_, navigation_handle->GetURL());
  start_url_is_home_page_ =
      IsUserHomePage(browser_context_, navigation_handle->GetURL());

  if (!started_in_foreground) {
    was_hidden_ = true;
    return CONTINUE_OBSERVING;
  }

  // When OnStart is invoked, we don't yet know whether we're observing a web
  // page load, vs another kind of load (e.g. a download or a PDF). Thus,
  // metrics and source information should not be recorded here. Instead, we
  // store data we might want to persist in member variables below, and later
  // record UKM metrics for that data once we've confirmed that we're observing
  // a web page load.

  effective_connection_type_ =
      network_quality_tracker_->GetEffectiveConnectionType();
  http_rtt_estimate_ = network_quality_tracker_->GetHttpRTT();
  transport_rtt_estimate_ = network_quality_tracker_->GetTransportRTT();
  downstream_kbps_estimate_ =
      network_quality_tracker_->GetDownstreamThroughputKbps();
  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  main_frame_request_redirect_count_++;
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
          CONTINUE_OBSERVING ||
      mime_type == kOfflinePreviewsMimeType) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  if (navigation_handle->IsInMainFrame()) {
    largest_contentful_paint_handler_.RecordMainFrameTreeNodeId(
        navigation_handle->GetFrameTreeNodeId());
  }
  if (navigation_handle->GetWebContents()->GetContentsMimeType() ==
      kOfflinePreviewsMimeType) {
    if (!IsOfflinePreview(navigation_handle->GetWebContents()))
      return STOP_OBSERVING;
  }
  connection_info_ = navigation_handle->GetConnectionInfo();
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (response_headers)
    http_response_code_ = response_headers->response_code();
  // The PageTransition for the navigation may be updated on commit.
  page_transition_ = navigation_handle->GetPageTransition();
  was_cached_ = navigation_handle->WasResponseCached();
  RecordNoStatePrefetchMetrics(navigation_handle, source_id);
  RecordGeneratedNavigationUKM(source_id, navigation_handle->GetURL());
  navigation_is_cross_process_ = !navigation_handle->IsSameProcess();
  navigation_entry_offset_ = navigation_handle->GetNavigationEntryOffset();
  main_document_sequence_number_ = navigation_handle->GetWebContents()
                                       ->GetController()
                                       .GetLastCommittedEntry()
                                       ->GetMainFrameDocumentSequenceNumber();
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!was_hidden_) {
    RecordPageLoadMetrics(base::TimeTicks::Now());
    RecordTimingMetrics(timing);
    RecordInputTimingMetrics();
  }
  ReportLayoutStability();
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!was_hidden_) {
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordTimingMetrics(timing);
    RecordInputTimingMetrics();
    was_hidden_ = true;
  }
  return CONTINUE_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  if (was_hidden_)
    return;
  RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);

  // Error codes have negative values, however we log net error code enum values
  // for UMA histograms using the equivalent positive value. For consistency in
  // UKM, we convert to a positive value here.
  int64_t net_error_code = static_cast<int64_t>(failed_load_info.error) * -1;
  DCHECK_GE(net_error_code, 0);
  ukm::builders::PageLoad(GetDelegate().GetSourceId())
      .SetNet_ErrorCode_OnFailedProvisionalLoad(net_error_code)
      .SetPageTiming_NavigationToFailedProvisionalLoad(
          failed_load_info.time_to_failed_provisional_load.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!was_hidden_) {
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordTimingMetrics(timing);
    RecordInputTimingMetrics();
  }
  ReportLayoutStability();
}

void UkmPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* content,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  if (was_hidden_)
    return;
  for (auto const& resource : resources) {
    network_bytes_ += resource->delta_bytes;

    if (blink::IsSupportedImageMimeType(resource->mime_type)) {
      image_total_bytes_ += resource->delta_bytes;
      if (!resource->is_main_frame_resource)
        image_subframe_bytes_ += resource->delta_bytes;
    } else if (media::IsSupportedMediaMimeType(resource->mime_type) ||
               base::StartsWith(resource->mime_type, "audio/",
                                base::CompareCase::SENSITIVE) ||
               base::StartsWith(resource->mime_type, "video/",
                                base::CompareCase::SENSITIVE)) {
      media_bytes_ += resource->delta_bytes;
    }

    // Only sum body lengths for completed resources.
    if (!resource->is_complete)
      continue;
    if (blink::IsSupportedJavascriptMimeType(resource->mime_type)) {
      js_decoded_bytes_ += resource->decoded_body_length;
      if (resource->decoded_body_length > js_max_decoded_bytes_)
        js_max_decoded_bytes_ = resource->decoded_body_length;
    }
    if (resource->cache_type !=
        page_load_metrics::mojom::CacheType::kNotCached) {
      cache_bytes_ += resource->encoded_body_length;
    }
  }
}

void UkmPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (was_hidden_)
    return;
  if (extra_request_complete_info.request_destination ==
      network::mojom::RequestDestination::kDocument) {
    DCHECK(!main_frame_timing_.has_value());
    main_frame_timing_ = *extra_request_complete_info.load_timing_info;
  }
}

void UkmPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ukm::builders::PageLoad builder(GetDelegate().GetSourceId());

  base::Optional<int64_t> rounded_site_engagement_score =
      GetRoundedSiteEngagementScore();
  if (rounded_site_engagement_score) {
    builder.SetSiteEngagementScore(rounded_site_engagement_score.value());
  }

  base::Optional<bool> third_party_cookie_blocking_enabled =
      GetThirdPartyCookieBlockingEnabled();
  if (third_party_cookie_blocking_enabled) {
    builder.SetThirdPartyCookieBlockingEnabledForSite(
        third_party_cookie_blocking_enabled.value());
    UMA_HISTOGRAM_BOOLEAN("Privacy.ThirdPartyCookieBlockingEnabledForSite",
                          third_party_cookie_blocking_enabled.value());
  }

  if (timing.input_to_navigation_start) {
    builder.SetExperimental_InputToNavigationStart(
        timing.input_to_navigation_start.value().InMilliseconds());
  }
  if (timing.parse_timing->parse_start) {
    builder.SetParseTiming_NavigationToParseStart(
        timing.parse_timing->parse_start.value().InMilliseconds());
  }
  if (timing.document_timing->dom_content_loaded_event_start) {
    builder.SetDocumentTiming_NavigationToDOMContentLoadedEventFired(
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds());
  }
  if (timing.document_timing->load_event_start) {
    builder.SetDocumentTiming_NavigationToLoadEventFired(
        timing.document_timing->load_event_start.value().InMilliseconds());
  }
  if (timing.paint_timing->first_paint) {
    builder.SetPaintTiming_NavigationToFirstPaint(
        timing.paint_timing->first_paint.value().InMilliseconds());
  }
  if (timing.paint_timing->first_contentful_paint) {
    builder.SetPaintTiming_NavigationToFirstContentfulPaint(
        timing.paint_timing->first_contentful_paint.value().InMilliseconds());
  }
  if (timing.paint_timing->first_meaningful_paint) {
    builder.SetExperimental_PaintTiming_NavigationToFirstMeaningfulPaint(
        timing.paint_timing->first_meaningful_paint.value().InMilliseconds());
  }
  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          largest_contentful_paint_handler_.MainFrameLargestContentfulPaint();
  if (main_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          main_frame_largest_contentful_paint.Time(), GetDelegate())) {
    builder.SetPaintTiming_NavigationToLargestContentfulPaint_MainFrame(
        main_frame_largest_contentful_paint.Time().value().InMilliseconds());
  }
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          largest_contentful_paint_handler_.MergeMainFrameAndSubframes();
  if (all_frames_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    builder.SetPaintTiming_NavigationToLargestContentfulPaint(
        all_frames_largest_contentful_paint.Time().value().InMilliseconds());
  }
  if (timing.interactive_timing->first_input_delay) {
    base::TimeDelta first_input_delay =
        timing.interactive_timing->first_input_delay.value();
    builder.SetInteractiveTiming_FirstInputDelay4(
        first_input_delay.InMilliseconds());
  }
  if (timing.interactive_timing->first_input_timestamp) {
    base::TimeDelta first_input_timestamp =
        timing.interactive_timing->first_input_timestamp.value();
    builder.SetInteractiveTiming_FirstInputTimestamp4(
        first_input_timestamp.InMilliseconds());
  }

  if (timing.interactive_timing->longest_input_delay) {
    base::TimeDelta longest_input_delay =
        timing.interactive_timing->longest_input_delay.value();
    builder.SetInteractiveTiming_LongestInputDelay4(
        longest_input_delay.InMilliseconds());
  }
  if (timing.interactive_timing->longest_input_timestamp) {
    base::TimeDelta longest_input_timestamp =
        timing.interactive_timing->longest_input_timestamp.value();
    builder.SetInteractiveTiming_LongestInputTimestamp4(
        longest_input_timestamp.InMilliseconds());
  }
  builder.SetCpuTime(total_foreground_cpu_time_.InMilliseconds());

  // Use a bucket spacing factor of 1.3 for bytes.
  builder.SetNet_CacheBytes2(ukm::GetExponentialBucketMin(cache_bytes_, 1.3));
  builder.SetNet_NetworkBytes2(
      ukm::GetExponentialBucketMin(network_bytes_, 1.3));

  // Use a bucket spacing factor of 10 for JS bytes.
  builder.SetNet_JavaScriptBytes(
      ukm::GetExponentialBucketMin(js_decoded_bytes_, 10));
  builder.SetNet_JavaScriptMaxBytes(
      ukm::GetExponentialBucketMin(js_max_decoded_bytes_, 10));

  builder.SetNet_ImageBytes(
      ukm::GetExponentialBucketMin(image_total_bytes_, 1.3));
  builder.SetNet_ImageSubframeBytes(
      ukm::GetExponentialBucketMin(image_subframe_bytes_, 1.3));
  builder.SetNet_MediaBytes(ukm::GetExponentialBucketMin(media_bytes_, 1.3));

  if (main_frame_timing_)
    ReportMainResourceTimingMetrics(timing, &builder);

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordPageLoadMetrics(
    base::TimeTicks app_background_time) {
  ukm::builders::PageLoad builder(GetDelegate().GetSourceId());
  base::Optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(GetDelegate(),
                                                      app_background_time);
  if (foreground_duration) {
    builder.SetPageTiming_ForegroundDuration(
        foreground_duration.value().InMilliseconds());
  }

  bool is_user_initiated_navigation =
      // All browser initiated page loads are user-initiated.
      GetDelegate().GetUserInitiatedInfo().browser_initiated ||

      // Renderer-initiated navigations are user-initiated if there is an
      // associated input event.
      GetDelegate().GetUserInitiatedInfo().user_input_event;

  builder.SetExperimental_Navigation_UserInitiated(
      is_user_initiated_navigation);

  // Convert to the EffectiveConnectionType as used in SystemProfileProto
  // before persisting the metric.
  metrics::SystemProfileProto::Network::EffectiveConnectionType
      proto_effective_connection_type =
          metrics::ConvertEffectiveConnectionType(effective_connection_type_);
  if (proto_effective_connection_type !=
      metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    builder.SetNet_EffectiveConnectionType2_OnNavigationStart(
        static_cast<int64_t>(proto_effective_connection_type));
  }

  if (http_response_code_) {
    builder.SetNet_HttpResponseCode(
        static_cast<int64_t>(http_response_code_.value()));
  }
  if (http_rtt_estimate_) {
    builder.SetNet_HttpRttEstimate_OnNavigationStart(
        static_cast<int64_t>(http_rtt_estimate_.value().InMilliseconds()));
  }
  if (transport_rtt_estimate_) {
    builder.SetNet_TransportRttEstimate_OnNavigationStart(
        static_cast<int64_t>(transport_rtt_estimate_.value().InMilliseconds()));
  }
  if (downstream_kbps_estimate_) {
    builder.SetNet_DownstreamKbpsEstimate_OnNavigationStart(
        static_cast<int64_t>(downstream_kbps_estimate_.value()));
  }
  // page_transition_ fits in a uint32_t, so we can safely cast to int64_t.
  builder.SetNavigation_PageTransition(static_cast<int64_t>(page_transition_));
  // GetDelegate().GetPageEndReason() fits in a uint32_t, so we can safely cast
  // to int64_t.
  builder.SetNavigation_PageEndReason(
      static_cast<int64_t>(GetDelegate().GetPageEndReason()));
  if (GetDelegate().DidCommit() && was_cached_) {
    builder.SetWasCached(1);
  }
  if (GetDelegate().DidCommit() && navigation_is_cross_process_) {
    builder.SetIsCrossProcessNavigation(navigation_is_cross_process_);
  }
  if (GetDelegate().DidCommit()) {
    builder.SetNavigationEntryOffset(navigation_entry_offset_);
    builder.SetMainDocumentSequenceNumber(main_document_sequence_number_);
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::ReportMainResourceTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    ukm::builders::PageLoad* builder) {
  DCHECK(main_frame_timing_.has_value());

  builder->SetMainFrameResource_SocketReused(main_frame_timing_->socket_reused);

  int64_t dns_start_ms =
      main_frame_timing_->connect_timing.dns_start.since_origin()
          .InMilliseconds();
  int64_t dns_end_ms = main_frame_timing_->connect_timing.dns_end.since_origin()
                           .InMilliseconds();
  int64_t connect_start_ms =
      main_frame_timing_->connect_timing.connect_start.since_origin()
          .InMilliseconds();
  int64_t connect_end_ms =
      main_frame_timing_->connect_timing.connect_end.since_origin()
          .InMilliseconds();
  int64_t request_start_ms =
      main_frame_timing_->request_start.since_origin().InMilliseconds();
  int64_t send_start_ms =
      main_frame_timing_->send_start.since_origin().InMilliseconds();
  int64_t receive_headers_end_ms =
      main_frame_timing_->receive_headers_end.since_origin().InMilliseconds();

  DCHECK_LE(dns_start_ms, dns_end_ms);
  DCHECK_LE(dns_end_ms, connect_start_ms);
  DCHECK_LE(dns_start_ms, connect_start_ms);
  DCHECK_LE(connect_start_ms, connect_end_ms);

  int64_t dns_duration_ms = dns_end_ms - dns_start_ms;
  int64_t connect_duration_ms = connect_end_ms - connect_start_ms;
  int64_t request_start_to_send_start_ms = send_start_ms - request_start_ms;
  int64_t send_start_to_receive_headers_end_ms =
      receive_headers_end_ms - send_start_ms;
  int64_t request_start_to_receive_headers_end_ms =
      receive_headers_end_ms - request_start_ms;

  builder->SetMainFrameResource_DNSDelay(dns_duration_ms);
  builder->SetMainFrameResource_ConnectDelay(connect_duration_ms);
  if (request_start_to_send_start_ms >= 0) {
    builder->SetMainFrameResource_RequestStartToSendStart(
        request_start_to_send_start_ms);
  }
  if (send_start_to_receive_headers_end_ms >= 0) {
    builder->SetMainFrameResource_SendStartToReceiveHeadersEnd(
        send_start_to_receive_headers_end_ms);
  }
  builder->SetMainFrameResource_RequestStartToReceiveHeadersEnd(
      request_start_to_receive_headers_end_ms);

  if (!main_frame_timing_->request_start.is_null() &&
      !GetDelegate().GetNavigationStart().is_null()) {
    base::TimeDelta navigation_start_to_request_start =
        main_frame_timing_->request_start - GetDelegate().GetNavigationStart();

    builder->SetMainFrameResource_NavigationStartToRequestStart(
        navigation_start_to_request_start.InMilliseconds());
  }

  if (!main_frame_timing_->receive_headers_start.is_null() &&
      !GetDelegate().GetNavigationStart().is_null()) {
    base::TimeDelta navigation_start_to_receive_headers_start =
        main_frame_timing_->receive_headers_start -
        GetDelegate().GetNavigationStart();
    builder->SetMainFrameResource_NavigationStartToReceiveHeadersStart(
        navigation_start_to_receive_headers_start.InMilliseconds());
  }

  if (connection_info_.has_value()) {
    page_load_metrics::NetworkProtocol protocol =
        page_load_metrics::GetNetworkProtocol(*connection_info_);
    if (IsSupportedProtocol(protocol)) {
      builder->SetMainFrameResource_HttpProtocolScheme(
          static_cast<int>(protocol));
    }
  }

  if (main_frame_request_redirect_count_ > 0) {
    builder->SetMainFrameResource_RedirectCount(
        main_frame_request_redirect_count_);
  }
}

void UkmPageLoadMetricsObserver::ReportLayoutStability() {
  ukm::builders::PageLoad(GetDelegate().GetSourceId())
      .SetLayoutInstability_CumulativeShiftScore(LayoutShiftUkmValue(
          GetDelegate().GetPageRenderData().layout_shift_score))
      .SetLayoutInstability_CumulativeShiftScore_MainFrame(LayoutShiftUkmValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score))
      .SetLayoutInstability_CumulativeShiftScore_MainFrame_BeforeInputOrScroll(
          LayoutShiftUkmValue(GetDelegate()
                                  .GetMainFrameRenderData()
                                  .layout_shift_score_before_input_or_scroll))
      .Record(ukm::UkmRecorder::Get());

  // TODO(crbug.com/1064483): We should move UMA recording to components/

  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.LayoutInstability.CumulativeShiftScore",
      LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));

  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame",
      LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));

  // Note: This depends on PageLoadMetrics internally processing loading
  // behavior before timing metrics if they come in the same IPC update.
  if (font_preload_started_before_rendering_observed_) {
    UMA_HISTOGRAM_COUNTS_100(
        "PageLoad.Clients.FontPreload.LayoutInstability.CumulativeShiftScore",
        LayoutShiftUmaValue(
            GetDelegate().GetPageRenderData().layout_shift_score));
  }
}

void UkmPageLoadMetricsObserver::RecordInputTimingMetrics() {
  ukm::builders::PageLoad(GetDelegate().GetSourceId())
      .SetInteractiveTiming_NumInputEvents(
          GetDelegate().GetPageInputTiming().num_input_events)
      .SetInteractiveTiming_TotalInputDelay(
          GetDelegate().GetPageInputTiming().total_input_delay.InMilliseconds())
      .SetInteractiveTiming_TotalAdjustedInputDelay(
          GetDelegate()
              .GetPageInputTiming()
              .total_adjusted_input_delay.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

base::Optional<int64_t>
UkmPageLoadMetricsObserver::GetRoundedSiteEngagementScore() const {
  if (!browser_context_)
    return base::nullopt;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  // UKM privacy requires the engagement score be rounded to nearest
  // value of 10.
  int64_t rounded_document_engagement_score =
      static_cast<int>(std::roundf(
          engagement_service->GetScore(GetDelegate().GetUrl()) / 10.0)) *
      10;

  DCHECK(rounded_document_engagement_score >= 0 &&
         rounded_document_engagement_score <=
             engagement_service->GetMaxPoints());

  return rounded_document_engagement_score;
}

base::Optional<bool>
UkmPageLoadMetricsObserver::GetThirdPartyCookieBlockingEnabled() const {
  if (!browser_context_)
    return base::nullopt;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  if (!cookie_settings->IsCookieControlsEnabled())
    return base::nullopt;

  return !cookie_settings->IsThirdPartyAccessAllowed(GetDelegate().GetUrl(),
                                                     nullptr /* source */);
}

void UkmPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  largest_contentful_paint_handler_.RecordTiming(timing.paint_timing,
                                                 subframe_rfh);
  bool loading_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("loading", &loading_enabled);
  if (!loading_enabled)
    return;
  const page_load_metrics::ContentfulPaintTimingInfo& paint =
      largest_contentful_paint_handler_.MergeMainFrameAndSubframes();

  if (paint.ContainsValidTime()) {
    TRACE_EVENT_INSTANT2(
        "loading",
        "NavStartToLargestContentfulPaint::Candidate::AllFrames::UKM",
        TRACE_EVENT_SCOPE_THREAD, "data", paint.DataAsTraceValue(),
        "main_frame_tree_node_id",
        largest_contentful_paint_handler_.MainFrameTreeNodeId());
  } else {
    TRACE_EVENT_INSTANT1(
        "loading",
        "NavStartToLargestContentfulPaint::"
        "Invalidate::AllFrames::UKM",
        TRACE_EVENT_SCOPE_THREAD, "main_frame_tree_node_id",
        largest_contentful_paint_handler_.MainFrameTreeNodeId());
  }
}

void UkmPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  largest_contentful_paint_handler_.OnDidFinishSubFrameNavigation(
      navigation_handle, GetDelegate());
}

void UkmPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (GetDelegate().GetVisibilityTracker().currently_in_foreground())
    total_foreground_cpu_time_ += timing.task_time;
}

void UkmPageLoadMetricsObserver::RecordNoStatePrefetchMetrics(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  prerender::PrerenderManager* const prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!prerender_manager)
    return;

  const std::vector<GURL>& redirects = navigation_handle->GetRedirectChain();

  base::TimeDelta prefetch_age;
  prerender::FinalStatus final_status;
  prerender::Origin prefetch_origin;

  bool nostate_prefetch_entry_found = prerender_manager->GetPrefetchInformation(
      navigation_handle->GetURL(), &prefetch_age, &final_status,
      &prefetch_origin);

  // Try the URLs from the redirect chain.
  if (!nostate_prefetch_entry_found) {
    for (const auto& url : redirects) {
      nostate_prefetch_entry_found = prerender_manager->GetPrefetchInformation(
          url, &prefetch_age, &final_status, &prefetch_origin);
      if (nostate_prefetch_entry_found)
        break;
    }
  }

  if (!nostate_prefetch_entry_found)
    return;

  ukm::builders::NoStatePrefetch builder(source_id);
  builder.SetPrefetchedRecently_PrefetchAge(
      ukm::GetExponentialBucketMinForUserTiming(prefetch_age.InMilliseconds()));
  builder.SetPrefetchedRecently_FinalStatus(final_status);
  builder.SetPrefetchedRecently_Origin(prefetch_origin);
  builder.Record(ukm::UkmRecorder::Get());
}

bool UkmPageLoadMetricsObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->GetOfflinePreviewItem();
#else
  return false;
#endif
}

void UkmPageLoadMetricsObserver::RecordGeneratedNavigationUKM(
    ukm::SourceId source_id,
    const GURL& committed_url) {
  bool final_url_is_home_page = IsUserHomePage(browser_context_, committed_url);
  bool final_url_is_default_search =
      IsDefaultSearchEngine(browser_context_, committed_url);

  if (!final_url_is_home_page && !final_url_is_default_search &&
      !start_url_is_home_page_ && !start_url_is_default_search_) {
    return;
  }

  ukm::builders::GeneratedNavigation builder(source_id);
  builder.SetFinalURLIsHomePage(final_url_is_home_page);
  builder.SetFinalURLIsDefaultSearchEngine(final_url_is_default_search);
  builder.SetFirstURLIsHomePage(start_url_is_home_page_);
  builder.SetFirstURLIsDefaultSearchEngine(start_url_is_default_search_);
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnLoadingBehaviorObserved(
    content::RenderFrameHost* rfh,
    int behavior_flag) {
  if (behavior_flag & blink::LoadingBehaviorFlag::
                          kLoadingBehaviorFontPreloadStartedBeforeRendering) {
    font_preload_started_before_rendering_observed_ = true;
  }
}
