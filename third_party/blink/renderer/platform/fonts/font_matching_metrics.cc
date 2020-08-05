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
HashSet<T> SetIntersection(const HashSet<T>& a, const HashSet<T>& b) {
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

void FontMatchingMetrics::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  OnFontLookup();
  uint64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupKey key(name, font_description.GetFontSelectionRequest());
  LocalFontLookupResult result{hash, check_type, is_loading_fallback};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data) {
  OnFontLookup();
  uint64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupKey key(fallback_character,
                         font_description.GetFontSelectionRequest());
  LocalFontLookupResult result{hash, check_type,
                               false /* is_loading_fallback */};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    LocalFontLookupType check_type,
    SimpleFontData* resulting_font_data) {
  OnFontLookup();
  uint64_t hash = GetHashForFontData(resulting_font_data);
  LocalFontLookupKey key(font_description.GetFontSelectionRequest());
  LocalFontLookupResult result{hash, check_type,
                               false /* is_loading_fallback */};
  font_lookups_.insert(key, result);
}

void FontMatchingMetrics::ReportFontFamilyLookupByGenericFamily(
    const AtomicString& generic_font_family_name,
    UScriptCode script,
    FontDescription::GenericFamilyType generic_family_type,
    const AtomicString& resulting_font_name) {
  OnFontLookup();
  GenericFontLookupKey key(generic_font_family_name, script,
                           generic_family_type);
  generic_font_lookups_.insert(key, resulting_font_name);
}

void FontMatchingMetrics::PublishIdentifiabilityMetrics() {
  for (const auto& entry : font_lookups_) {
    const LocalFontLookupKey& key = entry.key;
    const LocalFontLookupResult& result = entry.value;

    uint64_t input_digest = blink::IdentifiabilityDigestHelper(
        AtomicStringHash::GetHash(key.name), key.fallback_character,
        key.weight.RawValue(), key.width.RawValue(), key.slope.RawValue());
    uint64_t output_digest = blink::IdentifiabilityDigestHelper(
        result.hash, result.check_type, result.is_loading_fallback);

    blink::IdentifiabilityMetricBuilder(
        base::UkmSourceId::FromInt64(source_id_))
        .Set(IdentifiableSurface::FromTypeAndInput(
                 IdentifiableSurface::Type::kLocalFontLookup, input_digest),
             output_digest)
        .Record(ukm_recorder_);
  }
  font_lookups_.clear();

  for (const auto& entry : generic_font_lookups_) {
    const GenericFontLookupKey& key = entry.key;
    const AtomicString& result = entry.value;

    uint64_t input_digest = blink::IdentifiabilityDigestHelper(
        AtomicStringHash::GetHash(key.generic_font_family_name), key.script,
        key.generic_family_type);
    uint64_t output_digest =
        blink::IdentifiabilityDigestHelper(AtomicStringHash::GetHash(result));

    blink::IdentifiabilityMetricBuilder(
        base::UkmSourceId::FromInt64(source_id_))
        .Set(IdentifiableSurface::FromTypeAndInput(
                 IdentifiableSurface::Type::kGenericFontLookup, input_digest),
             output_digest)
        .Record(ukm_recorder_);
  }
  generic_font_lookups_.clear();
}

void FontMatchingMetrics::PublishUkmMetrics() {
  ukm::builders::FontMatchAttempts(source_id_)
      .SetLoadContext(top_level_ ? kTopLevel : kSubFrame)
      .SetSystemFontFamilySuccesses(ukm::GetExponentialBucketMin(
          SetIntersection(successful_font_families_, system_font_families_)
              .size(),
          kUkmFontLoadCountBucketSpacing))
      .SetSystemFontFamilyFailures(ukm::GetExponentialBucketMin(
          SetIntersection(failed_font_families_, system_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilySuccesses(ukm::GetExponentialBucketMin(
          SetIntersection(successful_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetWebFontFamilyFailures(ukm::GetExponentialBucketMin(
          SetIntersection(failed_font_families_, web_font_families_).size(),
          kUkmFontLoadCountBucketSpacing))
      .SetLocalFontFailures(ukm::GetExponentialBucketMin(
          local_fonts_failed_.size(), kUkmFontLoadCountBucketSpacing))
      .SetLocalFontSuccesses(ukm::GetExponentialBucketMin(
          local_fonts_succeeded_.size(), kUkmFontLoadCountBucketSpacing))
      .Record(ukm_recorder_);
}

void FontMatchingMetrics::OnFontLookup() {
  if (!time_of_earliest_unpublished_font_lookup_) {
    time_of_earliest_unpublished_font_lookup_ = base::Time::Now();
    return;
  }

  if (base::Time::Now() - *time_of_earliest_unpublished_font_lookup_ >=
      base::TimeDelta::FromMinutes(1)) {
    PublishIdentifiabilityMetrics();
    time_of_earliest_unpublished_font_lookup_ = base::Time::Now();
  }
}

void FontMatchingMetrics::PublishAllMetrics() {
  PublishIdentifiabilityMetrics();
  PublishUkmMetrics();
}

uint64_t FontMatchingMetrics::GetHashForFontData(SimpleFontData* font_data) {
  // TODO(alexmt) Implement when hash is available.
  return font_data ? 1 : 0;
}

}  // namespace blink
