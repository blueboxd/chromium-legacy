// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_

#include <type_traits>
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// The Encode/Decode families of functions are meant to be used to encode
// various types so that they can be specified via field trial configurations
// and Prefs.

bool DecodeIdentifiabilityType(const base::StringPiece,
                               blink::IdentifiableSurface*);

bool DecodeIdentifiabilityType(const base::StringPiece,
                               blink::IdentifiableSurface::Type*);

// This explosion of DecodeIdentifiabilityTypePair specializations is a great
// example of why you don't want to go down the path of templates if you could
// ever avoid it.
//
// The variability here is that unordered_map uses pair<K,V> as its value_type
// while map uses pair<const K, V>. Per spec, unordered_map should also use the
// latter, but alas, that is not the case.
//
// Adding to the confusion, pair<const K, V> is non-movable and non-copyable. So
// it can't be returned via a trivial *out parameter. Amazing.
template <typename U,
          typename V = typename U::value_type,
          typename P = typename std::remove_const<typename V::first_type>::type>
std::pair<V, bool> DecodeIdentifiabilityTypePair(base::StringPiece s) {
  auto pieces =
      base::SplitStringPiece(s, ";", base::WhitespaceHandling::TRIM_WHITESPACE,
                             base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2)
    return {V(), false};
  P from;
  int to;
  if (!DecodeIdentifiabilityType(pieces[0], &from) ||
      !base::StringToInt(pieces[1], &to))
    return {V(), false};
  if (to < 0 || to > IdentifiabilityStudySettings::kMaxSamplingRateDenominator)
    return {V(), false};
  return {V(from, to), true};
}

template <
    typename U,
    typename S = std::enable_if_t<
        std::is_same<typename U::key_type, typename U::value_type>::value>>
std::pair<typename U::value_type, bool> DecodeIdentifiabilityTypePair(
    base::StringPiece s) {
  using P = typename U::value_type;
  P value;
  if (!DecodeIdentifiabilityType(s, &value))
    return {P(), false};
  return {value, true};
}

std::string EncodeIdentifiabilityType(const blink::IdentifiableSurface&);

std::string EncodeIdentifiabilityType(const blink::IdentifiableSurface::Type&);

std::string EncodeIdentifiabilityType(
    const std::pair<const blink::IdentifiableSurface, int>&);

std::string EncodeIdentifiabilityType(
    const std::pair<const blink::IdentifiableSurface::Type, int>&);

std::string EncodeIdentifiabilityType(
    const std::pair<blink::IdentifiableSurface, int>&);

std::string EncodeIdentifiabilityType(
    const std::pair<blink::IdentifiableSurface::Type, int>&);

// Decodes a field trial parameter containing a list of values. The result is
// returned in the form of a container type that must be specified at
// instantiation. There should be a valid DecodeIdentifiabilityType
// specialization for the container's value type.
template <typename T,
          std::pair<typename T::value_type, bool> Decoder(
              const base::StringPiece) = DecodeIdentifiabilityTypePair<T>>
T DecodeIdentifiabilityFieldTrialParam(base::StringPiece encoded_value) {
  T result;
  auto hashes = base::SplitStringPiece(
      encoded_value, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const auto& hash : hashes) {
    auto decoded = Decoder(hash);
    if (!decoded.second)
      continue;
    result.insert(decoded.first);
  }
  return result;
}

// Encodes a field trial parameter that will contain a list of values taken from
// a container. The container must satisfy the named requirement Container. Its
// value_type must have a corresponding EncodeIdentifiabilityType
// specialization.
template <typename T,
          std::string Encoder(const typename T::value_type&) =
              EncodeIdentifiabilityType>
std::string EncodeIdentifiabilityFieldTrialParam(const T& source) {
  std::vector<std::string> result;
  std::transform(source.begin(), source.end(), std::back_inserter(result),
                 [](auto& v) { return Encoder(v); });
  return base::JoinString(result, ",");
}

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_
