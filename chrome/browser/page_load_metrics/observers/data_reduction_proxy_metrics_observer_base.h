// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_BASE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_BASE_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

namespace data_reduction_proxy {
class DataReductionProxyData;
class DataReductionProxyPingbackClient;

// Observer responsible for recording core page load metrics relevant to
// DataReductionProxy's pingback.
class DataReductionProxyMetricsObserverBase
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataReductionProxyMetricsObserverBase();
  ~DataReductionProxyMetricsObserverBase() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_compelte_info) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnEventOccurred(const void* const event_key) override;
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  // Exponentially bucket the number of bytes for privacy-implicated resources.
  // Input below 10KB returns 0.
  static int64_t ExponentiallyBucketBytes(int64_t bytes);

 protected:
  // Derived classes can override this method and treat it as they would
  // |OnCommit|. This is done so that internal state is set correctly in this'
  // OnCommit.
  virtual ObservePolicy OnCommitCalled(
      content::NavigationHandle* navigation_handle,
      ukm::SourceId source_id);

  void set_data(std::unique_ptr<DataReductionProxyData> data) {
    data_ = std::move(data);
  }
  void set_lite_page_redirect_penalty(base::TimeDelta penalty) {
    lite_page_redirect_penalty_ = penalty;
  }
  void set_lite_page_redirect_status(previews::ServerLitePageStatus status) {
    lite_page_redirect_status_ = status;
  }
  DataReductionProxyData* data() const { return data_.get(); }
  int num_network_resources() const { return num_network_resources_; }
  int num_data_reduction_proxy_resources() const {
    return num_data_reduction_proxy_resources_;
  }
  int64_t network_bytes_proxied() const { return network_bytes_proxied_; }
  int64_t insecure_network_bytes() const { return insecure_network_bytes_; }
  int64_t secure_network_bytes() const { return secure_network_bytes_; }
  int64_t insecure_original_network_bytes() const {
    return insecure_original_network_bytes_;
  }
  int64_t secure_original_network_bytes() const {
    return secure_original_network_bytes_;
  }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) final;

  // Records UKM for the data_reduction_proxy event.
  void RecordUKM(const page_load_metrics::PageLoadExtraInfo& info) const;

  // Sends the page load information to the pingback client.
  void SendPingback(const page_load_metrics::mojom::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info,
                    bool app_background_occurred);

  // Gets the default DataReductionProxyPingbackClient. Overridden in testing.
  virtual DataReductionProxyPingbackClient* GetPingbackClient() const;

  // Used as a callback to getting a memory dump of the related renderer
  // process.
  void ProcessMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> memory_dump);

  // Gets the memory coordinator for Chrome. Virtual for testing.
  virtual void RequestProcessDump(
      base::ProcessId pid,
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback);

  // Data related to this navigation.
  std::unique_ptr<DataReductionProxyData> data_;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_;

  // True if a Preview opt out occurred during this page load.
  bool opted_out_;

  // The number of resources that used data reduction proxy.
  int num_data_reduction_proxy_resources_;

  // The number of resources that did not come from cache.
  int num_network_resources_;

  // The total content network bytes that the user would have downloaded if they
  // were not using data reduction proxy for HTTP resources.
  int64_t insecure_original_network_bytes_;

  // The total content network bytes that the user would have downloaded if they
  // were not using data reduction proxy for HTTPS resources.
  int64_t secure_original_network_bytes_;

  // The total network bytes loaded through data reduction proxy. This value
  // only concerns HTTP traffic.
  int64_t network_bytes_proxied_;

  // The total network bytes used for HTTP resources.
  int64_t insecure_network_bytes_;

  // The total network bytes used for HTTPS resources.
  int64_t secure_network_bytes_;

  // The total cached bytes used for HTTP resources.
  int64_t insecure_cached_bytes_;

  // The total cached bytes used for HTTPS resources.
  int64_t secure_cached_bytes_;

  // The process ID of the main frame renderer during OnCommit.
  base::ProcessId process_id_;

  // The memory usage of the main frame renderer shortly after OnLoadEventStart.
  // Available after ProcessMemoryDump is called. 0 before that point.
  int64_t renderer_memory_usage_kb_;

  // A unique identifier to the child process of the render frame, stored in
  // case of a renderer crash.
  // Set at navigation commit time.
  int render_process_host_id_;

  // The number of touch events on the page.
  uint32_t touch_count_;

  // The number of scroll events on the page.
  uint32_t scroll_count_;

  // The number of main frame redirects that occurred before commit.
  uint32_t redirect_count_;

  // The time of the fetchStart of the main page HTML.
  base::Optional<base::TimeTicks> main_frame_fetch_start_;

  // The penalty of navigating to a lite page redirect preview.
  base::Optional<base::TimeDelta> lite_page_redirect_penalty_;

  // The status of an attempted lite page redirect preview.
  base::Optional<previews::ServerLitePageStatus> lite_page_redirect_status_;

  base::WeakPtrFactory<DataReductionProxyMetricsObserverBase> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserverBase);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_BASE_H_
