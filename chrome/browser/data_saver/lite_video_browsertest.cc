// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester.GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

class LiteVideoBrowserTest : public InProcessBrowserTest {
 public:
  explicit LiteVideoBrowserTest(bool enable_lite_mode = true,
                                bool enable_lite_video_feature = true)
      : enable_lite_mode_(enable_lite_mode) {
    std::vector<base::Feature> enabled_features;
    if (enable_lite_video_feature)
      enabled_features.push_back(features::kLiteVideo);

    std::vector<base::Feature> disabled_features = {
        // Disable fallback after decode error to avoid unexpected test pass on
        // the fallback path.
        media::kFallbackAfterDecodeError,

        // Disable out of process audio on Linux due to process spawn
        // failures. http://crbug.com/986021
        features::kAudioServiceOutOfProcess,
    };

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~LiteVideoBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    if (enable_lite_mode_)
      command_line->AppendSwitch("enable-spdy-proxy-auth");
    command_line->AppendSwitch(
        lite_video::switches::kLiteVideoForceOverrideDecision);
  }

  void SetUp() override {
    http_server_.ServeFilesFromSourceDirectory(media::GetTestDataPath());
    ASSERT_TRUE(http_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void TestMSEPlayback(const std::string& media_file,
                       const std::string& segment_duration,
                       const std::string& segment_fetch_delay_before_end) {
    base::StringPairs query_params;
    std::string media_files = media_file;
    // Add few media segments, separated by ';'
    media_files += ";" + media_file + "?id=1";
    media_files += ";" + media_file + "?id=2";
    media_files += ";" + media_file + "?id=3";
    media_files += ";" + media_file + "?id=4";
    query_params.emplace_back("mediaFile", media_files);
    query_params.emplace_back("mediaType",
                              media::GetMimeTypeForFile(media_file));
    query_params.emplace_back("MSESegmentDurationMS", segment_duration);
    query_params.emplace_back("MSESegmentFetchDelayBeforeEndMS",
                              segment_fetch_delay_before_end);
    RunMediaTestPage("mse_player.html", query_params,
                     base::ASCIIToUTF16(media::kEnded));
  }

  // Runs a html page with a list of URL query parameters.
  // The test starts a local http test server to load the test page
  void RunMediaTestPage(const std::string& html_page,
                        const base::StringPairs& query_params,
                        const base::string16& expected_title) {
    std::string query = media::GetURLQueryString(query_params);
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), http_server_.GetURL("/" + html_page + "?" + query)));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  bool enable_lite_mode_;  // Whether LiteMode is enabled.
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer http_server_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(LiteVideoBrowserTest);
};

IN_PROC_BROWSER_TEST_F(LiteVideoBrowserTest, SimplePlayback) {
  TestMSEPlayback("bear-vp9.webm", "2000", "2000");

  RetryForHistogramUntilCountReached(histogram_tester(),
                                     "Media.VideoHeight.Initial.MSE", 1);

  histogram_tester().ExpectUniqueSample("LiteVideo.HintAgent.HasHint", true, 1);
  histogram_tester().ExpectTotalCount("LiteVideo.URLLoader.ThrottleLatency", 4);
  histogram_tester().ExpectTotalCount(
      "LiteVideo.HintAgent.StopThrottleDueToBufferUnderflow", 0);
}

class LiteVideoWithLiteModeDisabledBrowserTest : public LiteVideoBrowserTest {
 public:
  LiteVideoWithLiteModeDisabledBrowserTest()
      : LiteVideoBrowserTest(false /*enable_lite_mode*/,
                             true /*enable_lite_video_feature*/) {}
};

IN_PROC_BROWSER_TEST_F(LiteVideoWithLiteModeDisabledBrowserTest,
                       VideoThrottleDisabled) {
  TestMSEPlayback("bear-vp9.webm", "2000", "2000");

  RetryForHistogramUntilCountReached(histogram_tester(),
                                     "Media.VideoHeight.Initial.MSE", 1);

  histogram_tester().ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester().ExpectTotalCount("LiteVideo.URLLoader.ThrottleLatency", 0);
}

class LiteVideoAndLiteModeDisabledBrowserTest : public LiteVideoBrowserTest {
 public:
  LiteVideoAndLiteModeDisabledBrowserTest()
      : LiteVideoBrowserTest(false /*enable_lite_mode*/,
                             false /*enable_lite_video_feature*/) {}
};

IN_PROC_BROWSER_TEST_F(LiteVideoAndLiteModeDisabledBrowserTest,
                       VideoThrottleDisabled) {
  TestMSEPlayback("bear-vp9.webm", "2000", "2000");

  RetryForHistogramUntilCountReached(histogram_tester(),
                                     "Media.VideoHeight.Initial.MSE", 1);

  histogram_tester().ExpectTotalCount("LiteVideo.HintAgent.HasHint", 0);
  histogram_tester().ExpectTotalCount("LiteVideo.URLLoader.ThrottleLatency", 0);
}

}  // namespace
