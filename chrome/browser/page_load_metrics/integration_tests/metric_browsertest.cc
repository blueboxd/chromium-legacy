// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/trace_event_analyzer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/tracing_controller.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using base::Bucket;
using base::CommandLine;
using base::HistogramTester;
using base::JSONReader;
using base::JSONWriter;
using base::OnceClosure;
using base::RunLoop;
using base::StringPiece;
using base::Value;
using base::trace_event::TraceConfig;
using content::TracingController;
using content::WebContents;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;
using ukm::TestAutoSetUkmRecorder;
using ukm::TestUkmRecorder;
using ukm::builders::PageLoad;
using ukm::mojom::UkmEntry;

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

void ExpectPageLoadMetric(const TestUkmRecorder& ukm_recorder,
                          StringPiece metric_name,
                          int64_t expected_value) {
  std::vector<const UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(PageLoad::kEntryName);
  auto name_filter = [&metric_name](const UkmEntry* entry) {
    return !TestUkmRecorder::EntryHasMetric(entry, metric_name);
  };
  entries.erase(std::remove_if(entries.begin(), entries.end(), name_filter),
                entries.end());
  EXPECT_EQ(1ul, entries.size());
  TestUkmRecorder::ExpectEntryMetric(entries[0], metric_name, expected_value);
}

}  // namespace

class MetricIntegrationTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "third_party/blink/web_tests/external/wpt");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 protected:
  void Serve(const std::string& url, const std::string& content) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest, url, content));
  }

  void Start() { ASSERT_TRUE(embedded_test_server()->Start()); }

  void Load(const std::string& relative_url) {
    GURL url = embedded_test_server()->GetURL("example.com", relative_url);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void LoadHTML(const std::string& content) {
    Serve("/test.html", content);
    Start();
    Load("/test.html");
  }

  void StartTracing(const std::vector<std::string>& categories) {
    RunLoop wait_for_tracing;
    TracingController::GetInstance()->StartTracing(
        TraceConfig(
            base::StringPrintf("{\"included_categories\": [\"%s\"]}",
                               base::JoinString(categories, "\", \"").c_str())),
        wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  void StopTracing(std::string& trace_output) {
    RunLoop wait_for_tracing;
    TracingController::GetInstance()->StopTracing(
        TracingController::CreateStringEndpoint(base::BindOnce(
            [](OnceClosure quit_closure, std::string* output,
               std::unique_ptr<std::string> trace_str) {
              *output = *trace_str;
              std::move(quit_closure).Run();
            },
            wait_for_tracing.QuitClosure(), &trace_output)));
    wait_for_tracing.Run();
  }

  std::unique_ptr<TraceAnalyzer> StopTracingAndAnalyze() {
    std::string trace_str;
    StopTracing(trace_str);
    return std::unique_ptr<TraceAnalyzer>(TraceAnalyzer::Create(trace_str));
  }

  WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpCommandLine(CommandLine* command_line) override {
    // Set a default window size for consistency.
    command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
  }

 private:
  static std::unique_ptr<HttpResponse> HandleRequest(
      const std::string& relative_url,
      const std::string& content,
      const HttpRequest& request) {
    if (request.relative_url != relative_url)
      return nullptr;
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("text/html; charset=utf-8");
    return std::move(response);
  }
};

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
  TestAutoSetUkmRecorder ukm_recorder;
  HistogramTester histogram_tester;

  // Check web perf API.
  double expected_score = EvalJs(web_contents(), "runtest()").ExtractDouble();

  // Check trace event.
  auto trace_scores = LayoutShiftScores(*StopTracingAndAnalyze());
  EXPECT_EQ(1u, trace_scores.size());
  EXPECT_EQ(expected_score, trace_scores[0]);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Check UKM.
  ExpectPageLoadMetric(ukm_recorder,
                       PageLoad::kLayoutInstability_CumulativeShiftScoreName,
                       LayoutShiftUkmValue(expected_score));

  // Check UMA.
  auto samples = histogram_tester.GetAllSamples(
      "PageLoad.LayoutInstability.CumulativeShiftScore");
  EXPECT_EQ(1ul, samples.size());
  EXPECT_EQ(samples[0], Bucket(LayoutShiftUmaValue(expected_score), 1));
}
