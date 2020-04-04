// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_policy.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace content {

namespace {

// Maximum number of allowed conversion metadata values. Higher entropy
// conversion metadata is stripped to these lower bits.
const int kMaxAllowedConversionValues = 8;

}  // namespace

uint64_t ConversionPolicy::NoiseProvider::GetNoisedConversionData(
    uint64_t conversion_data) const {
  // Return |conversion_data| without any noise 95% of the time.
  if (base::RandDouble() > .05)
    return conversion_data;

  // 5% of the time return a random number in the allowed range. Note that the
  // value is noised 5% of the time, but only wrong 5 *
  // (kMaxAllowedConversionValues - 1) / kMaxAllowedConversionValues percent of
  // the time.
  return static_cast<uint64_t>(base::RandInt(0, kMaxAllowedConversionValues));
}

// static
std::unique_ptr<ConversionPolicy> ConversionPolicy::CreateForTesting(
    std::unique_ptr<NoiseProvider> noise_provider) {
  return base::WrapUnique<ConversionPolicy>(
      new ConversionPolicy(std::move(noise_provider)));
}

ConversionPolicy::ConversionPolicy()
    : noise_provider_(std::make_unique<NoiseProvider>()) {}

ConversionPolicy::ConversionPolicy(
    std::unique_ptr<ConversionPolicy::NoiseProvider> noise_provider)
    : noise_provider_(std::move(noise_provider)) {}

ConversionPolicy::~ConversionPolicy() = default;

std::string ConversionPolicy::GetSanitizedConversionData(
    uint64_t conversion_data) const {
  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  conversion_data = noise_provider_->GetNoisedConversionData(conversion_data);

  // Allow at most 3 bits of entropy in conversion data. base::StringPrintf() is
  // used over base::HexEncode() because HexEncode() returns a hex string with
  // little-endian ordering. Big-endian ordering is expected here because the
  // API assumes big-endian when parsing attributes.
  return base::StringPrintf("%" PRIx64,
                            conversion_data % kMaxAllowedConversionValues);
}

}  // namespace content
