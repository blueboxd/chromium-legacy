// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/tuneable.h"

#include <random>

#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"

namespace {

// Random seed for all tuneables.
static std::string* GetRandomSeedPtr() {
  static base::NoDestructor<std::string> s_random_seed;
  return &(*s_random_seed);
}

// Generate a pseudorandom number that depends only on the random seed provided
// to SetRandomSeedForTuneables and the name provided as an argument.
//
// We cast |T| to / from int for the non-specialized implementation, because the
// underlying parameters sent by finch are ints anyway.  One can't really do
// too much better.  Since specific types must be declared explicitly for the
// Tuneable specializations anyway (see the bottom of this file), there's no
// chance of somebody picking something we haven't thought of and getting an
// unexpected specialization of GenerateRandom.
template <typename T>
T GenerateRandom(const char* name, T minimum_value, T maximum_value) {
  return static_cast<T>(GenerateRandom<int>(
      name, static_cast<int>(minimum_value), static_cast<int>(maximum_value)));
}

template <>
int GenerateRandom<int>(const char* name,
                        int minimum_value,
                        int maximum_value) {
  // It's okay if this isn't terribly random.
  auto name_hash =
      base::PersistentHash(std::string(name) + *GetRandomSeedPtr());
  // Limit to the range [min, max].
  // TODO(liberato): I think that this is biased.
  return name_hash % (maximum_value - minimum_value + 1) + minimum_value;
}

template <>
base::TimeDelta GenerateRandom<base::TimeDelta>(const char* name,
                                                base::TimeDelta minimum_value,
                                                base::TimeDelta maximum_value) {
  return base::TimeDelta::FromMilliseconds(GenerateRandom<int>(
      name, minimum_value.InMilliseconds(), maximum_value.InMilliseconds()));
}

// Get the finch parameter `name`_`suffix`, and clamp it to the given values.
// Return `default_value` if there is no parameter, or if the experiment is off.
template <typename T>
T GetParam(const char* name,
           const char* suffix,
           T minimum_value,
           T default_value,
           T maximum_value) {
  return static_cast<T>(GetParam<int>(
      name, suffix, static_cast<int>(minimum_value),
      static_cast<int>(default_value), static_cast<int>(maximum_value)));
}

template <>
int GetParam<int>(const char* name,
                  const char* suffix,
                  int minimum_value,
                  int default_value,
                  int maximum_value) {
  // TODO: "media_" # `name` ?  Seems like a good idea, since finch params are
  // not local to any finch feature.  For now, we let consumers do this.
  std::string param_name = std::string(name) + std::string(suffix);
  return base::ClampToRange(
      base::FeatureParam<int>(&::media::kMediaOptimizer, param_name.c_str(),
                              default_value)
          .Get(),
      minimum_value, maximum_value);
}

template <>
base::TimeDelta GetParam<base::TimeDelta>(const char* name,
                                          const char* suffix,
                                          base::TimeDelta minimum_value,
                                          base::TimeDelta default_value,
                                          base::TimeDelta maximum_value) {
  return base::TimeDelta::FromMilliseconds(GetParam<int>(
      name, suffix, minimum_value.InMilliseconds(),
      default_value.InMilliseconds(), maximum_value.InMilliseconds()));
}

}  // namespace

namespace media {

template <typename T>
Tuneable<T>::Tuneable(const char* name,
                      T minimum_value,
                      T default_value,
                      T maximum_value) {
  // Fetch the finch-provided range, clamped to the min, max and defaulted to
  // the hardcoded default.  This way, if it's unset, min == max == default.
  T finch_minimum =
      GetParam<T>(name, "_min", minimum_value, default_value, maximum_value);
  T finch_maximum =
      GetParam<T>(name, "_max", minimum_value, default_value, maximum_value);

  if (finch_minimum > finch_maximum) {
    // Bad parameters.  They're all in range, so we could pick any, but we
    // assume that the finch range has no meaning and just use the (hopefully)
    // saner default.
    t_ = default_value;
    return;
  }

  t_ = GenerateRandom<T>(name, finch_minimum, finch_maximum);
}

template <typename T>
Tuneable<T>::~Tuneable() = default;

void MEDIA_EXPORT
SetRandomSeedForTuneables(const base::UnguessableToken& seed) {
  *GetRandomSeedPtr() = seed.ToString();
}

// All allowed Tuneable types.  Be sure that GenerateRandom() and GetParam()
// do something sane for any type you add.
template class MEDIA_EXPORT Tuneable<int>;
template class MEDIA_EXPORT Tuneable<base::TimeDelta>;
template class MEDIA_EXPORT Tuneable<size_t>;

}  // namespace media
