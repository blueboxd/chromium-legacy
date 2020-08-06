// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"

namespace content {
class WebContents;
}

namespace extensions {

// This can be incorporated into an in-process browser test to help test
// which identifiability metrics got collected.
//
// Usage:
// 1) include as a member of test fixture, e.g.
// identifiability_metrics_test_helper_
// 2) Call SetUpOnMainThread() from fixture's SetUpOnMainThread().
// 3) In the test:
//    base::RunLoop run_loop;
//    identifiability_metrics_test_helper_.PrepareForTest(&run_loop);
//    /* do stuff */
//    auto metrics =
//        identifiability_metrics_test_helper_.NavigateToBlankAndWaitForMetrics(
//            web_contents, &run_loop);
//    /* check that metrics has the right stuff.
//       extensions::SurfaceForExtension may be useful here. */
class IdentifiabilityMetricsTestHelper {
 public:
  IdentifiabilityMetricsTestHelper();
  ~IdentifiabilityMetricsTestHelper();

  void SetUpOnMainThread();

  void PrepareForTest(base::RunLoop* run_loop);

  // Navigates to about:blank and returns metrics from the page that is
  // replaced.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
  NavigateToBlankAndWaitForMetrics(content::WebContents* contents,
                                   base::RunLoop* run_loop);

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_
