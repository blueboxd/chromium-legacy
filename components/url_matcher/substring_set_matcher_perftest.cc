// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/substring_set_matcher.h"

#include <limits>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace url_matcher {

namespace {

// Returns a character, cycling from 0 to the maximum signed char value.
char GetCurrentChar() {
  static char current = 0;
  if (current == std::numeric_limits<char>::max())
    current = 0;
  else
    current++;
  return current;
}

// Returns a string of the given length.
std::string GetString(size_t len) {
  std::vector<char> pattern;
  pattern.reserve(len);
  for (size_t j = 0; j < len; j++)
    pattern.push_back(GetCurrentChar());

  return std::string(pattern.begin(), pattern.end());
}

// Tests performance of SubstringSetMatcher for hundred thousand keys each of
// 100 characters.
TEST(SubstringSetMatcherPerfTest, HundredThousandKeys) {
  std::vector<StringPattern> patterns;

  // Create patterns.
  const size_t kNumPatterns = 100000;
  const size_t kPatternLen = 100;
  for (size_t i = 0; i < kNumPatterns; i++)
    patterns.emplace_back(GetString(kPatternLen), i);

  base::ElapsedTimer init_timer;

  // Allocate SubstringSetMatcher on the heap so that EstimateMemoryUsage below
  // also includes its stack allocated memory.
  auto matcher = std::make_unique<SubstringSetMatcher>(patterns);
  base::TimeDelta init_time = init_timer.Elapsed();

  // Match patterns against a string of 5000 characters.
  const size_t kTextLen = 5000;
  base::ElapsedTimer match_timer;
  std::set<StringPattern::ID> matches;
  matcher->Match(GetString(kTextLen), &matches);
  base::TimeDelta match_time = match_timer.Elapsed();

  const char* kInitializationTime = ".init_time";
  const char* kMatchTime = ".match_time";
  const char* kMemoryUsage = ".memory_usage";
  auto reporter = perf_test::PerfResultReporter("SubstringSetMatcher",
                                                "HundredThousandKeys");
  reporter.RegisterImportantMetric(kInitializationTime, "us");
  reporter.RegisterImportantMetric(kMatchTime, "us");
  reporter.RegisterImportantMetric(kMemoryUsage, "Mb");

  reporter.AddResult(kInitializationTime, init_time);
  reporter.AddResult(kMatchTime, match_time);
  reporter.AddResult(
      kMemoryUsage,
      (base::trace_event::EstimateMemoryUsage(matcher) * 1.0 / (1 << 20)));
}

}  // namespace

}  // namespace url_matcher
