// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "base/test/trace_event_analyzer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::Bucket;
using base::Value;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;
using ukm::builders::PageLoad;

namespace {

int64_t LayoutShiftUkmValue(float shift_score) {
  // Report (shift_score * 100) as an int in the range [0, 1000].
  return static_cast<int>(roundf(std::min(shift_score, 10.0f) * 100.0f));
}

int32_t LayoutShiftUmaValue(float shift_score) {
  // Report (shift_score * 10) as an int in the range [0, 100].
  return static_cast<int>(roundf(std::min(shift_score, 10.0f) * 10.0f));
}

std::vector<double> LayoutShiftScores(TraceAnalyzer& analyzer) {
  std::vector<double> scores;
  TraceEventVector events;
  analyzer.FindEvents(Query::EventNameIs("LayoutShift"), &events);
  for (auto* event : events) {
    std::unique_ptr<Value> data;
    event->GetArgAsValue("data", &data);
    scores.push_back(*data->FindDoubleKey("score"));
  }
  return scores;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LayoutInstability) {
  LoadHTML(R"HTML(
    <script src="/layout-instability/resources/util.js"></script>
    <script src="resources/testharness.js"></script>
    <script>
    // Tell testharness.js to not wait for 'real' tests; we only want
    // testharness.js for its assertion helpers.
    setup({'output': false});
    </script>

    <style>
    #shifter { position: relative; width: 300px; height: 200px; }
    </style>
    <div id="shifter"></div>
    <script>
    runtest = async () => {
      const watcher = new ScoreWatcher;

      // Wait for the initial render to complete.
      await waitForAnimationFrames(2);

      // Modify the position of the div.
      document.querySelector("#shifter").style = "top: 160px";

      // An element of size (300 x 200) has shifted by 160px.
      const expectedScore = computeExpectedScore(300 * (200 + 160), 160);

      // Observer fires after the frame is painted.
      assert_equals(watcher.score, 0, "The shift should not have happened yet");
      await watcher.promise;

      // Verify that the Performance API returns what we'd expect.
      assert_equals(watcher.score, expectedScore, "bad score");

      return expectedScore;
    };
    </script>
  )HTML");

  StartTracing({"loading"});

  // Check web perf API.
  double expected_score = EvalJs(web_contents(), "runtest()").ExtractDouble();

  // Check trace event.
  auto trace_scores = LayoutShiftScores(*StopTracingAndAnalyze());
  EXPECT_EQ(1u, trace_scores.size());
  EXPECT_EQ(expected_score, trace_scores[0]);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check UKM.
  ExpectUKMPageLoadMetric(PageLoad::kLayoutInstability_CumulativeShiftScoreName,
                          LayoutShiftUkmValue(expected_score));

  // Check UMA.
  auto samples = histogram_tester().GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(samples[0], Bucket(LayoutShiftUmaValue(expected_score), 1));
}
