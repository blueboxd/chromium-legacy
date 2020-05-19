// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/sim_hash.h"

#include "base/hash/legacy_hash.h"

#include <algorithm>

namespace federated_learning {

namespace {

uint64_t g_seed1 = 1ULL;
uint64_t g_seed2 = 2ULL;
constexpr double kTwoPi = 2.0 * 3.141592653589793;

// Hashes i and j to create a uniform RV in [0,1].
double RandomUniform(uint64_t i, uint64_t j, uint64_t seed) {
  uint64_t arr[2] = {i, j};
  uint64_t hashed = base::legacy::CityHash64WithSeed(
      base::as_bytes(
          base::make_span(reinterpret_cast<const char*>(arr), sizeof(arr))),
      seed);

  return static_cast<double>(hashed) /
         static_cast<double>(std::numeric_limits<uint64_t>::max());
}

// Hashes i and j to create two uniform RVs in [0,1]. Uses the Box-Muller
// transform to generate a Gaussian.
double RandomGaussian(uint64_t i, uint64_t j) {
  double rv1 = RandomUniform(i, j, g_seed1);
  double rv2 = RandomUniform(j, i, g_seed2);

  // BoxMuller
  return std::sqrt(-2.0 * std::log(rv1)) * std::cos(kTwoPi * rv2);
}

}  // namespace

LargeBitVector::LargeBitVector() = default;
LargeBitVector::LargeBitVector(const LargeBitVector&) = default;
LargeBitVector::~LargeBitVector() = default;

void LargeBitVector::SetBit(uint64_t pos) {
  positions_of_set_bits_.insert(pos);
}

const std::set<uint64_t>& LargeBitVector::PositionsOfSetBits() const {
  return positions_of_set_bits_;
}

void SetSeedsForTesting(uint64_t seed1, uint64_t seed2) {
  g_seed1 = seed1;
  g_seed2 = seed2;
}

uint64_t SimHashBits(const LargeBitVector& input, size_t output_dimensions) {
  DCHECK_LT(0u, output_dimensions);
  DCHECK_LE(output_dimensions, 64u);

  uint64_t result = 0;
  for (size_t d = 0; d < output_dimensions; ++d) {
    double acc = 0;

    for (uint64_t pos : input.PositionsOfSetBits())
      acc += RandomGaussian(d, pos);

    if (acc > 0)
      result |= (1ULL << d);
  }

  return result;
}

uint64_t SimHashStrings(const std::unordered_set<std::string>& input,
                        size_t output_dimensions) {
  DCHECK_LT(0u, output_dimensions);
  DCHECK_LE(output_dimensions, 64u);

  LargeBitVector large_bit_vector;
  for (const std::string& s : input) {
    large_bit_vector.SetBit(
        base::legacy::CityHash64(base::as_bytes(base::make_span(s))));
  }

  return SimHashBits(large_bit_vector, output_dimensions);
}

}  // namespace federated_learning
