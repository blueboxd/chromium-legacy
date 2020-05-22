// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/field_trial_param_conversions.h"
#include <utility>

bool DecodeIdentifiabilityType(const base::StringPiece s,
                               blink::IdentifiableSurface* out) {
  uint64_t hash = 0;
  if (!base::StringToUint64(s, &hash))
    return false;
  *out = blink::IdentifiableSurface::FromMetricHash(hash);
  return true;
}

bool DecodeIdentifiabilityType(const base::StringPiece s,
                               blink::IdentifiableSurface::Type* out) {
  uint64_t hash = 0;
  if (!base::StringToUint64(s, &hash))
    return false;
  if ((hash & blink::IdentifiableSurface::kTypeMask) != hash)
    return false;
  *out = static_cast<blink::IdentifiableSurface::Type>(hash);
  return true;
}

std::string EncodeIdentifiabilityType(const blink::IdentifiableSurface& s) {
  return base::NumberToString(s.ToUkmMetricHash());
}

std::string EncodeIdentifiabilityType(
    const blink::IdentifiableSurface::Type& t) {
  return base::NumberToString(static_cast<uint64_t>(t));
}

std::string EncodeIdentifiabilityType(
    const std::pair<const blink::IdentifiableSurface, int>& v) {
  return EncodeIdentifiabilityType(v.first) + ";" +
         base::NumberToString(v.second);
}

std::string EncodeIdentifiabilityType(
    const std::pair<const blink::IdentifiableSurface::Type, int>& v) {
  return EncodeIdentifiabilityType(v.first) + ";" +
         base::NumberToString(v.second);
}
