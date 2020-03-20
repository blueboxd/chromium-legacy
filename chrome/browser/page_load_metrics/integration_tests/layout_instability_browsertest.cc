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

void CheckRect(const Value& list_value, int x, int y, int width, int height) {
  auto list = list_value.GetList();
  EXPECT_EQ(list[0].GetInt(), x);
  EXPECT_EQ(list[1].GetInt(), y);
  EXPECT_EQ(list[2].GetInt(), width);
  EXPECT_EQ(list[3].GetInt(), height);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, LayoutInstability) {
  Start();
  Load("/layout-instability.html");
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

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, CLSAttribution_Enclosure) {
  LoadHTML(R"HTML(
    <script src="/layout-instability/resources/util.js"></script>
    <style>
    body { margin: 0; }
    #shifter {
      position: relative; background: #def;
      width: 300px; height: 200px;
    }
    #inner {
      position: relative; background: #f97;
      width: 100px; height: 100px;
    }
    #absfollow {
      position: absolute; background: #ffd; opacity: 50%;
      width: 350px; height: 200px; left: 0; top: 160px;
    }
    .stateB { top: 160px; }
    .stateB #inner { left: 100px; }
    .stateC ~ #absfollow { top: 0; }
    </style>
    <div id="shifter" class="stateA">
      <div id="inner"></div>
    </div>
    <div id="absfollow"></div>
    <script>
    runTest = async () => {
      await waitForAnimationFrames(2);
      document.querySelector("#shifter").className = "stateB";
      await waitForAnimationFrames(2);
      document.querySelector("#shifter").className = "stateC";
      await waitForAnimationFrames(2);
    };
    </script>
  )HTML");

  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  ASSERT_TRUE(EvalJs(web_contents(), "runTest()").error.empty());
  std::unique_ptr<TraceAnalyzer> analyzer = StopTracingAndAnalyze();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventNameIs("LayoutShift"), &events);
  EXPECT_EQ(2ul, events.size());

  // Shift of #inner ignored as redundant, fully enclosed by #shifter.

  std::unique_ptr<Value> shift_data1;
  events[0]->GetArgAsValue("data", &shift_data1);
  auto impacted_nodes1 = shift_data1->FindListKey("impacted_nodes")->GetList();
  EXPECT_EQ(1ul, impacted_nodes1.size());

  const Value& node_data1 = impacted_nodes1[0];
  EXPECT_NE(*node_data1.FindIntKey("node_id"), 0);
  CheckRect(*node_data1.FindListKey("old_rect"), 0, 0, 300, 200);
  CheckRect(*node_data1.FindListKey("new_rect"), 0, 160, 300, 200);

  // Shift of #shifter ignored as redundant, fully enclosed by #follow.

  std::unique_ptr<Value> shift_data2;
  events[1]->GetArgAsValue("data", &shift_data2);
  auto impacted_nodes2 = shift_data2->FindListKey("impacted_nodes")->GetList();
  EXPECT_EQ(1ul, impacted_nodes2.size());

  const Value& node_data2 = impacted_nodes2[0];
  EXPECT_NE(*node_data2.FindIntKey("node_id"), 0);
  CheckRect(*node_data2.FindListKey("old_rect"), 0, 160, 350, 200);
  CheckRect(*node_data2.FindListKey("new_rect"), 0, 0, 350, 200);
}

IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, CLSAttribution_MaxImpact) {
  LoadHTML(R"HTML(
    <script src="/layout-instability/resources/util.js"></script>
    <style>
    body { margin: 0; }
    #a, #b, #c, #d, #e, #f {
      display: inline-block;
      background: gray;
      min-width: 10px;
      min-height: 10px;
      vertical-align: top;
    }
    #a { width: 30px; height: 30px; }
    #b { width: 20px; height: 20px; }
    #c { height: 50px; }
    #d { width: 50px; }
    #e { width: 40px; height: 30px; }
    #f { width: 30px; height: 40px; }
    </style>
    <div id="grow"></div>
    <div id="a"></div
    ><div id="b"></div
    ><div id="c"></div
    ><div id="d"></div
    ><div id="e"></div
    ><div id="f"></div>
    <script>
    runTest = async () => {
      await waitForAnimationFrames(2);
      document.querySelector("#grow").style.height = "50px";
      await waitForAnimationFrames(2);
    };
    </script>
  )HTML");

  StartTracing({"loading", TRACE_DISABLED_BY_DEFAULT("layout_shift.debug")});
  ASSERT_TRUE(EvalJs(web_contents(), "runTest()").error.empty());
  std::unique_ptr<TraceAnalyzer> analyzer = StopTracingAndAnalyze();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventNameIs("LayoutShift"), &events);
  EXPECT_EQ(1ul, events.size());

  std::unique_ptr<Value> shift_data;
  events[0]->GetArgAsValue("data", &shift_data);
  auto impacted = shift_data->FindListKey("impacted_nodes")->GetList();
  EXPECT_EQ(5ul, impacted.size());

  // #f should replace #b, the smallest div.
  CheckRect(*impacted[0].FindListKey("new_rect"), 0, 50, 30, 30);    // #a
  CheckRect(*impacted[1].FindListKey("new_rect"), 150, 50, 30, 40);  // #f
  CheckRect(*impacted[2].FindListKey("new_rect"), 50, 50, 10, 50);   // #c
  CheckRect(*impacted[3].FindListKey("new_rect"), 60, 50, 50, 10);   // #d
  CheckRect(*impacted[4].FindListKey("new_rect"), 110, 50, 40, 30);  // #e

  ASSERT_EQ("DIV id='a'", *impacted[0].FindStringKey("debug_name"));
  ASSERT_EQ("DIV id='f'", *impacted[1].FindStringKey("debug_name"));
  ASSERT_EQ("DIV id='c'", *impacted[2].FindStringKey("debug_name"));
  ASSERT_EQ("DIV id='d'", *impacted[3].FindStringKey("debug_name"));
  ASSERT_EQ("DIV id='e'", *impacted[4].FindStringKey("debug_name"));
}
