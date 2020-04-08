/*
 * Copyright 2018 Google LLC.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sample_error.h"

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "symmetric_encryption.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

using uint_m = rlwe::MontgomeryInt<rlwe::Uint16>;

const int kTestingRounds = 100;
const int num_coeffs = 1000;
const std::vector<rlwe::Uint64> variances = {8, 15, 29, 50};

TEST(UtilsTest, CheckUpperBoundOnNoise) {
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);
  ASSERT_OK_AND_ASSIGN(auto modulus_params,
                       uint_m::Params::Create(rlwe::kNewhopeModulus));

  for (auto variance : variances) {
    for (int i = 0; i < kTestingRounds; i++) {
      ASSERT_OK_AND_ASSIGN(
          std::vector<uint_m> error,
          rlwe::SampleFromErrorDistribution<uint_m>(
              num_coeffs, variance, prng.get(), modulus_params.get()));
      // Check that each coefficient is in [-2*variance, 2*variance]
      for (int j = 0; j < num_coeffs; j++) {
        int reduced = error[j].ExportInt(modulus_params.get());
        if (reduced > (modulus_params->modulus >> 1)) {
          reduced = reduced - modulus_params->modulus;
        }
        EXPECT_LT(abs(reduced), 2 * variance + 1);
      }
    }
  }
}

TEST(UtilsTest, FailOnTooLargeVariance) {
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);
  ASSERT_OK_AND_ASSIGN(auto modulus_params,
                       uint_m::Params::Create(rlwe::kNewhopeModulus));
  rlwe::Uint64 variance = rlwe::kMaxVariance + 1;
  EXPECT_THAT(rlwe::SampleFromErrorDistribution<uint_m>(
                  num_coeffs, variance, prng.get(), modulus_params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(absl::StrCat("The variance, ", variance,
                                              ", must be at most ",
                                              rlwe::kMaxVariance))));
}

}  // namespace
