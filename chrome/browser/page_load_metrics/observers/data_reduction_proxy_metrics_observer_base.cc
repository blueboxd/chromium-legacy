// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "crypto/sha2.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

PageloadMetrics_PageEndReason ConvertPLMPageEndReasonToProto(
    page_load_metrics::PageEndReason reason) {
  switch (reason) {
    case page_load_metrics::END_NONE:
      return PageloadMetrics_PageEndReason_END_NONE;
    case page_load_metrics::END_RELOAD:
      return PageloadMetrics_PageEndReason_END_RELOAD;
    case page_load_metrics::END_FORWARD_BACK:
      return PageloadMetrics_PageEndReason_END_FORWARD_BACK;
    case page_load_metrics::END_CLIENT_REDIRECT:
      return PageloadMetrics_PageEndReason_END_CLIENT_REDIRECT;
    case page_load_metrics::END_NEW_NAVIGATION:
      return PageloadMetrics_PageEndReason_END_NEW_NAVIGATION;
    case page_load_metrics::END_STOP:
      return PageloadMetrics_PageEndReason_END_STOP;
    case page_load_metrics::END_CLOSE:
      return PageloadMetrics_PageEndReason_END_CLOSE;
    case page_load_metrics::END_PROVISIONAL_LOAD_FAILED:
      return PageloadMetrics_PageEndReason_END_PROVISIONAL_LOAD_FAILED;
    case page_load_metrics::END_RENDER_PROCESS_GONE:
      return PageloadMetrics_PageEndReason_END_RENDER_PROCESS_GONE;
    default:
      return PageloadMetrics_PageEndReason_END_OTHER;
  }
}

uint64_t ComputeDataReductionProxyUUID(
    data_reduction_proxy::DataReductionProxyData* data) {
  if (!data || data->session_key().empty() || !data->page_id().has_value() ||
      data->page_id().value() == 0) {
    return 0;
  }

  uint64_t page_id = data->page_id().value();
  char buf[8];
  base::WriteBigEndian<uint64_t>(buf, page_id);

  std::string to_hash(data->session_key());
  to_hash.append(std::begin(buf), std::end(buf));

  char hash[32];
  crypto::SHA256HashString(base::StringPiece(to_hash), hash, 32);

  uint64_t uuid;
  base::ReadBigEndian<uint64_t>(hash, &uuid);
  return uuid;
}

}  // namespace

DataReductionProxyMetricsObserverBase::DataReductionProxyMetricsObserverBase()
    : browser_context_(nullptr),
      opted_out_(false),
      num_data_reduction_proxy_resources_(0),
      num_network_resources_(0),
      insecure_original_network_bytes_(0),
      secure_original_network_bytes_(0),
      network_bytes_proxied_(0),
      insecure_network_bytes_(0),
      secure_network_bytes_(0),
      insecure_cached_bytes_(0),
      secure_cached_bytes_(0),
      process_id_(base::kNullProcessId),
      renderer_memory_usage_kb_(0),
      render_process_host_id_(content::ChildProcessHost::kInvalidUniqueID),
      touch_count_(0),
      scroll_count_(0),
      redirect_count_(0) {}

DataReductionProxyMetricsObserverBase::
    ~DataReductionProxyMetricsObserverBase() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This BrowserContext is valid for the lifetime of
  // DataReductionProxyMetricsObserverBase. BrowserContext is always valid and
  // non-nullptr in NavigationControllerImpl, which is a member of WebContents.
  // A raw pointer to BrowserContext taken at this point will be valid until
  // after WebContent's destructor. The latest that PageLoadTracker's destructor
  // will be called is in MetricsWebContentsObserver's destrcutor, which is
  // called in WebContents destructor.
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();

  process_id_ = navigation_handle->GetWebContents()
                    ->GetMainFrame()
                    ->GetProcess()
                    ->GetProcess()
                    .Pid();
  render_process_host_id_ = navigation_handle->GetWebContents()
                                ->GetMainFrame()
                                ->GetProcess()
                                ->GetID();

  return OnCommitCalled(navigation_handle, source_id);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  redirect_count_++;
  return CONTINUE_OBSERVING;
}

// Check if the NavigationData indicates anything about the DataReductionProxy.
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnCommitCalled(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<DataReductionProxyData> data;
  auto* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context_);
  if (settings) {
    data = settings->CreateDataFromNavigationHandle(
        navigation_handle, navigation_handle->GetResponseHeaders());
  }
  if (!data || !(data->used_data_reduction_proxy() ||
                 data->was_cached_data_reduction_proxy_response())) {
    return STOP_OBSERVING;
  }
  data_ = std::move(data);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  previews::PreviewsUserData* previews_data = nullptr;

  if (ui_tab_helper)
    previews_data = ui_tab_helper->GetPreviewsUserData(navigation_handle);

  if (previews_data) {
    data_->set_black_listed(previews_data->black_listed_for_lite_page());
  }

  // DataReductionProxy page loads should only occur on HTTP navigations.
  DCHECK(!navigation_handle->GetURL().SchemeIsCryptographic());
  DCHECK_EQ(data_->request_url(), navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we send a pingback with data collected up to this point.
  if (info.did_commit) {
    SendPingback(timing, info, true /* app_background_occurred */);
    RecordUKM(info);
  }
  return STOP_OBSERVING;
}

void DataReductionProxyMetricsObserverBase::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendPingback(timing, info, false /* app_background_occurred */);
  RecordUKM(info);
}

// static
int64_t DataReductionProxyMetricsObserverBase::ExponentiallyBucketBytes(
    int64_t bytes) {
  const int64_t start_buckets = 10000;
  if (bytes < start_buckets) {
    return 0;
  }
  return ukm::GetExponentialBucketMin(bytes, 1.16);
}

void DataReductionProxyMetricsObserverBase::RecordUKM(
    const page_load_metrics::PageLoadExtraInfo& info) const {
  if (!data())
    return;

  if (!data()->used_data_reduction_proxy())
    return;

  int64_t original_network_bytes =
      insecure_original_network_bytes() + secure_original_network_bytes();
  uint64_t uuid = ComputeDataReductionProxyUUID(data());

  ukm::builders::DataReductionProxy builder(info.source_id);

  builder.SetEstimatedOriginalNetworkBytes(
      ExponentiallyBucketBytes(original_network_bytes));
  builder.SetDataSaverPageUUID(uuid);

  builder.Record(ukm::UkmRecorder::Get());
}

void DataReductionProxyMetricsObserverBase::SendPingback(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info,
    bool app_background_occurred) {
  // TODO(ryansturm): Move to OnFirstBackgroundEvent to handle some fast
  // shutdown cases. crbug.com/618072
  if (!browser_context_ || !data_)
    return;
  // Only consider timing events that happened before the first background
  // event.
  base::Optional<base::TimeDelta> response_start;
  base::Optional<base::TimeDelta> load_event_start;
  base::Optional<base::TimeDelta> first_image_paint;
  base::Optional<base::TimeDelta> first_contentful_paint;
  base::Optional<base::TimeDelta> experimental_first_meaningful_paint;
  base::Optional<base::TimeDelta> first_input_delay;
  base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;
  base::Optional<base::TimeDelta> parse_stop;
  base::Optional<base::TimeDelta> page_end_time;
  base::Optional<base::TimeDelta> main_frame_fetch_start;
  if (WasStartedInForegroundOptionalEventInForeground(timing.response_start,
                                                      info)) {
    response_start = timing.response_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    load_event_start = timing.document_timing->load_event_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, info)) {
    first_image_paint = timing.paint_timing->first_image_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    first_contentful_paint = timing.paint_timing->first_contentful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    experimental_first_meaningful_paint =
        timing.paint_timing->first_meaningful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_delay, info)) {
    first_input_delay = timing.interactive_timing->first_input_delay;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_blocked_on_script_load_duration, info)) {
    parse_blocked_on_script_load_duration =
        timing.parse_timing->parse_blocked_on_script_load_duration;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    parse_stop = timing.parse_timing->parse_stop;
  }
  if (GetDelegate()->DidCommit() && main_frame_fetch_start_) {
    main_frame_fetch_start =
        main_frame_fetch_start_.value() - GetDelegate()->GetNavigationStart();
  }
  if (info.started_in_foreground && info.page_end_time.has_value()) {
    // This should be reported even when the app goes into the background which
    // is excluded in |WasStartedInForegroundOptionalEventInForeground|.
    page_end_time = info.page_end_time;
  } else if (info.started_in_foreground) {
    page_end_time = base::TimeTicks::Now() - info.navigation_start;
  }

  // If a crash happens, report the host |render_process_host_id_| to the
  // pingback client. Otherwise report kInvalidUniqueID.
  int host_id = content::ChildProcessHost::kInvalidUniqueID;
  if (info.page_end_reason ==
      page_load_metrics::PageEndReason::END_RENDER_PROCESS_GONE) {
    host_id = render_process_host_id_;
  }

  const int64_t original_network_bytes =
      insecure_original_network_bytes_ +
      ExponentiallyBucketBytes(secure_original_network_bytes_);
  const int64_t network_bytes =
      insecure_network_bytes_ + ExponentiallyBucketBytes(secure_network_bytes_);
  const int64_t total_page_size_bytes =
      insecure_network_bytes_ + insecure_cached_bytes_ +
      ExponentiallyBucketBytes(secure_network_bytes_ + secure_cached_bytes_);

  // Recording cached bytes can be done with raw data, but the end result must
  // be bucketed in 50 linear buckets between 0% - 100%.
  const int64_t cached_bytes = insecure_cached_bytes_ + secure_cached_bytes_;
  const int64_t total_bytes =
      cached_bytes + insecure_network_bytes_ + secure_network_bytes_;
  int cached_percentage;
  if (total_bytes <= 0) {
    cached_percentage = 0;
  } else {
    cached_percentage =
        static_cast<int>(std::lround(static_cast<float>(cached_bytes) /
                                     static_cast<float>(total_bytes) * 100.0));
  }
  DCHECK_GE(cached_percentage, 0);
  DCHECK_LE(cached_percentage, 100);
  cached_percentage = cached_percentage - (cached_percentage % 2);
  const float cached_fraction = static_cast<float>(cached_percentage) / 100.0;

  DataReductionProxyPageLoadTiming data_reduction_proxy_timing(
      timing.navigation_start, response_start, load_event_start,
      first_image_paint, first_contentful_paint,
      experimental_first_meaningful_paint, first_input_delay,
      parse_blocked_on_script_load_duration, parse_stop, page_end_time,
      lite_page_redirect_penalty_, lite_page_redirect_status_,
      main_frame_fetch_start, network_bytes, original_network_bytes,
      total_page_size_bytes, cached_fraction, app_background_occurred,
      opted_out_, renderer_memory_usage_kb_, host_id,
      ConvertPLMPageEndReasonToProto(info.page_end_reason), touch_count_,
      scroll_count_, redirect_count_);
  GetPingbackClient()->SendPingback(*data_, data_reduction_proxy_timing);
}

void DataReductionProxyMetricsObserverBase::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_id_ != base::kNullProcessId) {
    auto callback = base::BindRepeating(
        &DataReductionProxyMetricsObserverBase::ProcessMemoryDump,
        weak_ptr_factory_.GetWeakPtr());
    RequestProcessDump(process_id_, callback);
  }
}

void DataReductionProxyMetricsObserverBase::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extra_request_complete_info.resource_type ==
      content::ResourceType::kMainFrame) {
    main_frame_fetch_start_ =
        extra_request_complete_info.load_timing_info->request_start;
  }
}

void DataReductionProxyMetricsObserverBase::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto const& resource : resources) {
    if (resource->cache_type == page_load_metrics::mojom::CacheType::kMemory)
      continue;
    if (resource->cache_type !=
        page_load_metrics::mojom::CacheType::kNotCached) {
      if (resource->is_complete) {
        if (resource->is_secure_scheme) {
          secure_cached_bytes_ += resource->encoded_body_length;
        } else {
          insecure_cached_bytes_ += resource->encoded_body_length;
        }
      }
      continue;
    }
    int64_t original_network_bytes =
        resource->delta_bytes *
        resource->data_reduction_proxy_compression_ratio_estimate;
    if (resource->is_secure_scheme) {
      secure_original_network_bytes_ += original_network_bytes;
      secure_network_bytes_ += resource->delta_bytes;
    } else {
      insecure_original_network_bytes_ += original_network_bytes;
      insecure_network_bytes_ += resource->delta_bytes;
    }
    if (resource->is_complete)
      num_network_resources_++;
    // If the request is proxied on a page with data saver proxy for the main
    // frame request, then it is very likely a data saver proxy for this
    // request.
    if (resource->proxy_used) {
      if (resource->is_complete)
        num_data_reduction_proxy_resources_++;
      // Proxied bytes are always non-secure.
      network_bytes_proxied_ += resource->delta_bytes;
    }
  }
}

DataReductionProxyPingbackClient*
DataReductionProxyMetricsObserverBase::GetPingbackClient() const {
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
             browser_context_)
      ->data_reduction_proxy_service()
      ->pingback_client();
}

void DataReductionProxyMetricsObserverBase::OnEventOccurred(
    const void* const event_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_key == PreviewsUITabHelper::OptOutEventKey())
    opted_out_ = true;
}

void DataReductionProxyMetricsObserverBase::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (event.GetType() == blink::WebInputEvent::kMouseDown ||
      event.GetType() == blink::WebInputEvent::kGestureTap) {
    touch_count_++;
  }

  if (event.GetType() == blink::WebInputEvent::kMouseWheel ||
      event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
      event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    scroll_count_++;
  }
}

void DataReductionProxyMetricsObserverBase::ProcessMemoryDump(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> memory_dump) {
  if (!success || !memory_dump)
    return;
  // There should only be one process in the dump.
  DCHECK_EQ(1, std::distance(memory_dump->process_dumps().begin(),
                             memory_dump->process_dumps().end()));

  auto process_dump_it = memory_dump->process_dumps().begin();
  if (process_dump_it == memory_dump->process_dumps().end())
    return;

  // We want to catch this in debug but not crash in release.
  DCHECK_EQ(process_id_, process_dump_it->pid());
  if (process_dump_it->pid() != process_id_)
    return;
  renderer_memory_usage_kb_ =
      static_cast<int64_t>(process_dump_it->os_dump().private_footprint_kb);
}

void DataReductionProxyMetricsObserverBase::RequestProcessDump(
    base::ProcessId pid,
    memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
        callback) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(pid, std::move(callback));
}

}  // namespace data_reduction_proxy
