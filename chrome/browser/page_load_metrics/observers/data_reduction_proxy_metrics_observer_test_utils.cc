// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_test_utils.h"

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "content/public/test/web_contents_tester.h"

namespace data_reduction_proxy {

previews::PreviewsUserData* PreviewsDataForNavigationHandle(
    content::NavigationHandle* navigation_handle) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (previews_user_data)
    return previews_user_data;
  return ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
      navigation_handle, 1u);
}

page_load_metrics::mojom::ResourceDataUpdatePtr
CreateDataReductionProxyResource(bool was_cached,
                                 int64_t delta_bytes,
                                 bool is_complete,
                                 bool proxy_used,
                                 double compression_ratio) {
  auto resource_data_update =
      page_load_metrics::mojom::ResourceDataUpdate::New();
  resource_data_update->cache_type =
      was_cached ? page_load_metrics::mojom::CacheType::kHttp
                 : page_load_metrics::mojom::CacheType::kNotCached;
  resource_data_update->delta_bytes = was_cached ? 0 : delta_bytes;
  resource_data_update->encoded_body_length = delta_bytes;
  resource_data_update->is_complete = is_complete;
  resource_data_update->proxy_used = true;
  resource_data_update->data_reduction_proxy_compression_ratio_estimate =
      compression_ratio;
  return resource_data_update;
}

TestPingbackClient::TestPingbackClient()
    : DataReductionProxyPingbackClientImpl(nullptr,
                                           base::ThreadTaskRunnerHandle::Get(),
                                           "unknown"),
      send_pingback_called_(false) {}

TestPingbackClient::~TestPingbackClient() {}

void TestPingbackClient::SendPingback(
    const DataReductionProxyData& data,
    const DataReductionProxyPageLoadTiming& timing) {
  timing_.reset(new DataReductionProxyPageLoadTiming(timing));
  send_pingback_called_ = true;
  data_ = data.DeepCopy();
}

void TestPingbackClient::Reset() {
  send_pingback_called_ = false;
  timing_.reset();
}

FakeInputEvent::FakeInputEvent(blink::WebInputEvent::Type type)
    : WebInputEvent(sizeof(FakeInputEvent),
                    type,
                    blink::WebInputEvent::kNoModifiers,
                    base::TimeTicks::Now()) {}

DataReductionProxyMetricsObserverTestBase::
    DataReductionProxyMetricsObserverTestBase()
    : pingback_client_(new TestPingbackClient()),
      data_reduction_proxy_used_(false),
      is_using_lite_page_(false),
      opt_out_expected_(false),
      black_listed_(false) {}

DataReductionProxyMetricsObserverTestBase::
    ~DataReductionProxyMetricsObserverTestBase() {}

void DataReductionProxyMetricsObserverTestBase::ResetTest() {
  page_load_metrics::InitPageLoadTimingForTest(&timing_);
  // Reset to the default testing state. Does not reset histogram state.
  timing_.navigation_start = base::Time::FromDoubleT(1);
  timing_.response_start = base::TimeDelta::FromSeconds(2);
  timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(3);
  timing_.paint_timing->first_contentful_paint =
      base::TimeDelta::FromSeconds(4);
  timing_.paint_timing->first_paint = base::TimeDelta::FromSeconds(4);
  timing_.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromSeconds(8);
  timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(5);
  timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
  timing_.parse_timing->parse_stop = base::TimeDelta::FromSeconds(4);
  timing_.parse_timing->parse_blocked_on_script_load_duration =
      base::TimeDelta::FromSeconds(1);
  PopulateRequiredTimingFields(&timing_);
}

void DataReductionProxyMetricsObserverTestBase::RunTest(
    bool data_reduction_proxy_used,
    bool is_using_lite_page,
    bool opt_out_expected,
    bool black_listed) {
  data_reduction_proxy_used_ = data_reduction_proxy_used;
  is_using_lite_page_ = is_using_lite_page;
  opt_out_expected_ = opt_out_expected;
  black_listed_ = black_listed;
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing_);
  pingback_client_->Reset();
}

void DataReductionProxyMetricsObserverTestBase::
    RunTestAndNavigateToUntrackedUrl(bool data_reduction_proxy_used,
                                     bool is_using_lite_page,
                                     bool opt_out_expected) {
  RunTest(data_reduction_proxy_used, is_using_lite_page, opt_out_expected,
          false);
  NavigateToUntrackedUrl();
}

void DataReductionProxyMetricsObserverTestBase::RunLitePageRedirectTest(
    previews::PreviewsUserData::ServerLitePageInfo* preview_info,
    net::EffectiveConnectionType ect) {
  preview_info_ = preview_info;
  ect_ = ect;
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing_);
  pingback_client_->Reset();
}

void DataReductionProxyMetricsObserverTestBase::SimulateRendererCrash() {
  observer()->RenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_ABNORMAL_TERMINATION);
}

// Verify that, if expected and actual are set, their values are equal.
// Otherwise, verify that both are unset.
void DataReductionProxyMetricsObserverTestBase::ExpectEqualOrUnset(
    const base::Optional<base::TimeDelta>& expected,
    const base::Optional<base::TimeDelta>& actual) {
  if (expected && actual) {
    EXPECT_EQ(expected.value(), actual.value());
  } else {
    EXPECT_TRUE(!expected);
    EXPECT_TRUE(!actual);
  }
}

void DataReductionProxyMetricsObserverTestBase::ValidateTimes() {
  EXPECT_TRUE(pingback_client_->send_pingback_called());
  EXPECT_EQ(timing_.navigation_start,
            pingback_client_->timing()->navigation_start);
  EXPECT_GT(pingback_client_->timing()->page_end_time, base::TimeDelta());
  ExpectEqualOrUnset(timing_.paint_timing->first_contentful_paint,
                     pingback_client_->timing()->first_contentful_paint);
  ExpectEqualOrUnset(
      timing_.paint_timing->first_meaningful_paint,
      pingback_client_->timing()->experimental_first_meaningful_paint);
  ExpectEqualOrUnset(timing_.response_start,
                     pingback_client_->timing()->response_start);
  ExpectEqualOrUnset(timing_.document_timing->load_event_start,
                     pingback_client_->timing()->load_event_start);
  ExpectEqualOrUnset(timing_.paint_timing->first_image_paint,
                     pingback_client_->timing()->first_image_paint);
  EXPECT_EQ(opt_out_expected_, pingback_client_->timing()->opt_out_occurred);
  EXPECT_EQ(timing_.document_timing->load_event_start
                ? static_cast<int64_t>(kMemoryKb)
                : 0,
            pingback_client_->timing()->renderer_memory_usage_kb);
}

void DataReductionProxyMetricsObserverTestBase::ValidateBlackListInPingback(
    bool black_listed) {
  EXPECT_TRUE(pingback_client_->send_pingback_called());
  EXPECT_EQ(black_listed, pingback_client_->data().black_listed());
}

void DataReductionProxyMetricsObserverTestBase::
    ValidatePreviewsStateInPingback() {
  EXPECT_EQ(!!preview_info_, pingback_client_->send_pingback_called());
  if (preview_info_) {
    EXPECT_EQ(preview_info_->drp_session_key,
              pingback_client_->data().session_key());
    EXPECT_EQ(preview_info_->page_id, pingback_client_->data().page_id());
    EXPECT_EQ(ect_, pingback_client_->data().effective_connection_type());
    EXPECT_EQ(preview_info_->status,
              pingback_client_->timing()->lite_page_redirect_status.value());
    // This is tested better in PreviewsLitePageRedirectMetricsObserverTest.
    EXPECT_GT(pingback_client_->timing()->lite_page_redirect_penalty.value(),
              base::TimeDelta());
  }
}

void DataReductionProxyMetricsObserverTestBase::ValidateRendererCrash(
    bool renderer_crashed) {
  EXPECT_TRUE(pingback_client_->send_pingback_called());
  EXPECT_EQ(renderer_crashed, pingback_client_->timing()->host_id !=
                                  content::ChildProcessHost::kInvalidUniqueID);
}

void DataReductionProxyMetricsObserverTestBase::SetUp() {
  page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
  PreviewsUITabHelper::CreateForWebContents(web_contents());
}

}  // namespace data_reduction_proxy
