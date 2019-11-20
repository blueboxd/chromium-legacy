// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_test_util.h"

#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"

std::string GetScriptLog(Browser* browser) {
  std::string script_log;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(), "sendLogToTest()",
      &script_log));
  return script_log;
}

void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}
