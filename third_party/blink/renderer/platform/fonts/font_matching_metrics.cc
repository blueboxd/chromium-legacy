// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"

#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

constexpr double kUkmFontLoadCountBucketSpacing = 1.3;

enum FontLoadContext { kTopLevel = 0, kSubFrame };

template <typename T>
HashSet<T> Intersection(const HashSet<T>& a, const HashSet<T>& b) {
  HashSet<T> result;
  for (const T& a_value : a) {
    if (b.Contains(a_value))
      result.insert(a_value);
  }
  return result;
}

}  // namespace

namespace blink {

FontMatchingMetrics::FontMatchingMetrics(bool top_level,
                                         ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id)
    : top_level_(top_level),
      ukm_recorder_(ukm_recorder),
      source_id_(source_id) {
  // Estimate of average page font use from anecdotal browsing session.
  constexpr unsigned kEstimatedFontCount = 7;
  local_fonts_succeeded_.ReserveCapacityForSize(kEstimatedFontCount);
  local_fonts_failed_.ReserveCapacityForSize(kEstimatedFontCount);
}

void FontMatchingMetrics::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  successful_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  failed_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportSystemFontFamily(
    const AtomicString& font_family_name) {
  system_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportWebFontFamily(
    const AtomicString& font_family_name) {
  web_font_families_.insert(font_family_name);
}

void FontMatchingMetrics::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  local_fonts_succeeded_.insert(font_name);
}

void FontMatchingMetrics::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  local_fonts_failed_.insert(font_name);
}

void FontMatchingMetrics::PublishUkmMetrics() {
  ukm::builders::FontMatchAttempts(source_id_)
      .SetLoadContext(top_level_ ? kTopLevel : kSubFrame)
      .SetSystemFontFamilySuccesses(ukm::GetExponentialBucketMin(
          Intersection(successful_font_families_, system_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetSystemFontFamilyFailures(ukm::GetExponentialBucketMin(
          Intersection(failed_font_families_, system_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilySuccesses(ukm::GetExponentialBucketMin(
          Intersection(successful_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilyFailures(ukm::GetExponentialBucketMin(
          Intersection(failed_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetLocalFontFailures(ukm::GetExponentialBucketMin(
          local_fonts_failed_.size(), kUkmFontLoadCountBucketSpacing))
      .SetLocalFontSuccesses(ukm::GetExponentialBucketMin(
          local_fonts_succeeded_.size(), kUkmFontLoadCountBucketSpacing))
      .Record(ukm_recorder_);
}

}  // namespace blink
