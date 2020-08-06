// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/identifiability_metrics_test_util.h"

#include "base/run_loop.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace extensions {

IdentifiabilityMetricsTestHelper::IdentifiabilityMetricsTestHelper() {
  privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters());
}

IdentifiabilityMetricsTestHelper::~IdentifiabilityMetricsTestHelper() = default;

void IdentifiabilityMetricsTestHelper::SetUpOnMainThread() {
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
}

void IdentifiabilityMetricsTestHelper::PrepareForTest(base::RunLoop* run_loop) {
  DCHECK(ukm_recorder_) << "IdentifiabilityMetricsTestHelper::"
                           "SetUpOnMainThread hasn't been called";
  ukm_recorder_->SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName, run_loop->QuitClosure());
}

std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
IdentifiabilityMetricsTestHelper::NavigateToBlankAndWaitForMetrics(
    content::WebContents* contents,
    base::RunLoop* run_loop) {
  DCHECK(ukm_recorder_) << "IdentifiabilityMetricsTestHelper::"
                           "SetUpOnMainThread hasn't been called";

  // Need to navigate away to force a metrics flush; otherwise it would be
  // dependent on periodic flush heuristics.
  content::NavigateToURLBlockUntilNavigationsComplete(contents,
                                                      GURL("about:blank"), 1);
  run_loop->Run();
  return ukm_recorder_->GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
}

}  // namespace extensions
