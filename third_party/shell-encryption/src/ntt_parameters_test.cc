/*
 * Copyright 2017 Google LLC.
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

#include "ntt_parameters.h"

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "constants.h"
#include "montgomery.h"
#include "status_macros.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"

using uint16_m = rlwe::MontgomeryInt<rlwe::Uint16>;
using uint64_m = rlwe::MontgomeryInt<rlwe::Uint64>;

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

namespace {

static const rlwe::Uint64 kLogN = rlwe::kNewhopeLogDegreeBound;
static const rlwe::Uint64 kN = rlwe::kNewhopeDegreeBound;

class NttParametersTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params14_,
                         uint16_m::Params::Create(rlwe::kNewhopeModulus));
  }
  std::unique_ptr<uint16_m::Params> params14_;
};

TEST_F(NttParametersTest, LogNumCoeffsTooLarge) {
  ASSERT_OK_AND_ASSIGN(auto params59,
                       uint64_m::Params::Create(rlwe::kModulus59));
  int log_n = rlwe::kMaxLogNumCoeffs + 1;
  EXPECT_THAT(
      rlwe::InitializeNttParameters<uint64_m>(log_n, params59.get()),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat("log_n, ", log_n, ", must be less than ",
                                      rlwe::kMaxLogNumCoeffs, "."))));

  log_n = (sizeof(typename uint16_m::Int) * 8) - 1;
  EXPECT_THAT(
      rlwe::InitializeNttParameters<uint16_m>(log_n, params14_.get()),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "log_n, ", log_n,
                   ", does not fit into underlying ModularInt::Int type."))));
}

TEST_F(NttParametersTest, PrimitiveNthRootOfUnity) {
  unsigned int log_ns[] = {2u, 4u, 6u, 8u, 11u};
  unsigned int len = 5;

  for (unsigned int i = 0; i < len; i++) {
    ASSERT_OK_AND_ASSIGN(uint16_m w,
                         rlwe::internal::PrimitiveNthRootOfUnity<uint16_m>(
                             log_ns[i], params14_.get()));
    unsigned int n = 1 << log_ns[i];

    // Ensure it is really a n-th root of unity.
    auto res = w.ModExp(n, params14_.get());
    auto one = uint16_m::ImportOne(params14_.get());
    EXPECT_EQ(res, one) << "Not an n-th root of unity.";

    // Ensure it is really a primitive n-th root of unity.
    auto res2 = w.ModExp(n / 2, params14_.get());
    EXPECT_NE(res2, one) << "Not a primitive n-th root of unity.";
  }
}

TEST_F(NttParametersTest, NttPsis) {
  // The values of psi should be the powers of the primitive 2n-th root of
  // unity.
  // Obtain the psis.
  ASSERT_OK_AND_ASSIGN(
      std::vector<uint16_m> psis,
      rlwe::internal::NttPsis<uint16_m>(kLogN, params14_.get()));

  // Verify that that the 0th entry is 1.
  uint16_m one = uint16_m::ImportOne(params14_.get());
  EXPECT_EQ(one, psis[0]);

  // Verify that the 1th entry is a primitive 2n-th root of unity.
  auto r1 = psis[1].ModExp(kN << 1, params14_.get());
  auto r2 = psis[1].ModExp(kN, params14_.get());
  EXPECT_EQ(one, r1);
  EXPECT_NE(one, r2);

  // Verify that each subsequent entry is the appropriate power of the 1th
  // entry.
  for (unsigned int i = 2; i < kN; i++) {
    auto ri = psis[1].ModExp(i, params14_.get());
    EXPECT_EQ(psis[i], ri);
  }
}

TEST_F(NttParametersTest, NttPsisBitrev) {
  // The values of psi should be bitreversed.
  // Target vector: obtain the psis in bitreversed order.
  ASSERT_OK_AND_ASSIGN(std::vector<uint16_m> psis_bitrev,
                       rlwe::NttPsisBitrev<uint16_m>(kLogN, params14_.get()));
  // Obtain the psis.
  ASSERT_OK_AND_ASSIGN(
      std::vector<uint16_m> psis,
      rlwe::internal::NttPsis<uint16_m>(kLogN, params14_.get()));
  // Obtain the mapping for bitreversed order
  std::vector<unsigned int> bit_rev = rlwe::internal::BitrevArray(kLogN);

  for (unsigned int i = 0; i < kN; i++) {
    EXPECT_EQ(psis_bitrev[i], psis[bit_rev[i]]);
  }
}

TEST_F(NttParametersTest, NttPsisInvBitrev) {
  // The values of the vectors should be psi^(-(brv[k]+1) for all k.
  // Target vector: obtain the psi inv in bit reversed order.
  ASSERT_OK_AND_ASSIGN(
      std::vector<uint16_m> psis_inv_bitrev,
      rlwe::NttPsisInvBitrev<uint16_m>(kLogN, params14_.get()));
  // Obtain the psis.
  ASSERT_OK_AND_ASSIGN(
      std::vector<uint16_m> psis,
      rlwe::internal::NttPsis<uint16_m>(kLogN, params14_.get()));
  // Obtain the mapping for bitreversed order
  std::vector<unsigned int> bit_rev = rlwe::internal::BitrevArray(kLogN);

  for (unsigned int i = 0; i < kN; i++) {
    EXPECT_EQ(params14_->One(), psis_inv_bitrev[i]
                                    .Mul(psis[1], params14_.get())
                                    .Mul(psis[bit_rev[i]], params14_.get())
                                    .ExportInt(params14_.get()));
  }
}

TEST_F(NttParametersTest, Bitrev) {
  for (unsigned int log_N = 2; log_N < 11; log_N++) {
    unsigned int N = 1 << log_N;
    std::vector<unsigned int> bit_rev = rlwe::internal::BitrevArray(log_N);

    // Visit each entry of the array.
    for (unsigned int i = 0; i < N; i++) {
      for (unsigned int j = 0; j < log_N; j++) {
        // Ensure bit j of i is equal to bit (log_N - j) of bit_rev[i].
        rlwe::Uint64 mask1 = 1 << j;
        rlwe::Uint64 mask2 = 1 << (log_N - j - 1);
        EXPECT_EQ((i & mask1) == 0, (bit_rev[i] & mask2) == 0);
      }
    }
  }
}

TEST_F(NttParametersTest, IncorrectNTTParams) {
  // Constants for this test.
  const int log_num_coeffs = 10;

  // 29-bit working modulus + 2, will no longer be 1 mod 2*n
  ASSERT_OK_AND_ASSIGN(auto params29,
                       uint64_m::Params::Create(rlwe::kModulus29 + 2));

  EXPECT_THAT(
      rlwe::InitializeNttParameters<uint64_m>(log_num_coeffs, params29.get()),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat("modulus is not 1 mod 2n for logn, ",
                                      log_num_coeffs))));
}

// Test all the NTT Parameter fields under 64-bit modulus.
TEST_F(NttParametersTest, Initialize) {
  ASSERT_OK_AND_ASSIGN(auto params59,
                       uint64_m::Params::Create(rlwe::kModulus59));
  ASSERT_OK_AND_ASSIGN(
      rlwe::NttParameters<uint64_m> ntt_params59,
      rlwe::InitializeNttParameters<uint64_m>(kLogN, params59.get()));

  uint64_m one = uint64_m::ImportOne(params59.get());

  // Obtain the mapping for bitreversed order
  std::vector<unsigned int> bit_rev = rlwe::internal::BitrevArray(kLogN);

  // Test first entry of psis in bitreversed order is one.
  EXPECT_EQ(one, ntt_params59.psis_bitrev[0]);

  // Test n/2-th (brv[1]-th) entry of psis in bitreversed order is a primitive
  // 2n-th root of unity.
  auto psi = ntt_params59.psis_bitrev[bit_rev[1]];
  auto r1 = psi.ModExp(kN << 1, params59.get());
  auto r2 = psi.ModExp(kN, params59.get());
  EXPECT_EQ(one, r1);
  EXPECT_NE(one, r2);

  // The values of psis should be the powers of the primitive 2n-th root of
  // unity in bitreversed order.
  for (unsigned int i = 0; i < kN; i++) {
    auto bi = psi.ModExp(i, params59.get());
    EXPECT_EQ(ntt_params59.psis_bitrev[bit_rev[i]], bi);
  }

  // Test psis_inv_bitrev contains the inverses of the powers of psi in
  // bitreversed order, each multiplied by the inverse of psi.
  for (unsigned int i = 0; i < 1 << kLogN; i++) {
    EXPECT_EQ(params59->One(),
              ntt_params59.psis_bitrev[i]
                  .Mul(psi, params59.get())
                  .Mul(ntt_params59.psis_inv_bitrev[i], params59.get())
                  .ExportInt(params59.get()));
  }
}

}  // namespace
