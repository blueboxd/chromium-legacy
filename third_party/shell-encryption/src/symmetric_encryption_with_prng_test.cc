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

#include "symmetric_encryption_with_prng.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "prng/integral_prng_types.h"
#include "status_macros.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_utils.h"

namespace rlwe {
namespace {

// Set constants.
const unsigned int kTestingRounds = 10;

// Useful typedefs.
using uint_m = MontgomeryInt<Uint32>;
using Polynomial = Polynomial<uint_m>;
using Key = SymmetricRlweKey<uint_m>;

class SymmetricEncryptionWithPrngTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params14_,
                         rlwe::testing::ConstructMontgomeryIntParams());
    ASSERT_OK_AND_ASSIGN(
        ntt_params_, InitializeNttParameters<uint_m>(rlwe::testing::kLogCoeffs,
                                                     params14_.get()));
    ASSERT_OK_AND_ASSIGN(
        auto temp_error_params,
        ErrorParams<uint_m>::Create(rlwe::testing::kDefaultLogT,
                                    rlwe::testing::kDefaultVariance,
                                    params14_.get(), &ntt_params_));
    error_params_ = absl::make_unique<ErrorParams<uint_m>>(temp_error_params);
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey() {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(
        rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
        rlwe::testing::kDefaultLogT, params14_.get(), &ntt_params_, prng.get());
  }

  rlwe::StatusOr<std::vector<Polynomial>> ConvertPlaintextsToNtt(
      const std::vector<std::vector<uint_m::Int>>& coeffs) {
    std::vector<Polynomial> ntt_plaintexts;
    for (int i = 0; i < coeffs.size(); ++i) {
      RLWE_ASSIGN_OR_RETURN(auto mont,
                            rlwe::testing::ConvertToMontgomery<uint_m>(
                                coeffs[i], params14_.get()));
      ntt_plaintexts.push_back(
          Polynomial::ConvertToNtt(mont, ntt_params_, params14_.get()));
    }
    return ntt_plaintexts;
  }

  void TestCompressedEncryptionDecryption(
      const std::vector<std::vector<uint_m::Int>>& plaintexts) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
    ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                         SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(std::string prng_encryption_seed,
                         SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng_encryption,
                         SingleThreadPrng::Create(prng_encryption_seed));
    ASSERT_OK_AND_ASSIGN(std::vector<Polynomial> ntt_plaintexts,
                         ConvertPlaintextsToNtt(plaintexts));
    ASSERT_OK_AND_ASSIGN(
        auto compressed_ciphertexts,
        EncryptWithPrng<uint_m>(key, ntt_plaintexts, prng.get(),
                                prng_encryption.get()));
    EXPECT_EQ(plaintexts.size(), compressed_ciphertexts.size());
    ASSERT_OK_AND_ASSIGN(auto another_prng,
                         SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(
        auto ciphertexts,
        ExpandFromPrng<uint_m>(compressed_ciphertexts, key.ModulusParams(),
                               key.NttParams(), error_params_.get(),
                               another_prng.get()));
    EXPECT_EQ(plaintexts.size(), ciphertexts.size());
    for (int i = 0; i < ciphertexts.size(); ++i) {
      // Expect that the error of an expanded ciphertext is of a fresh
      // encryption.
      EXPECT_EQ(ciphertexts[i].Error(), error_params_->B_encryption());
      ASSERT_OK_AND_ASSIGN(auto decrypted,
                           Decrypt<uint_m>(key, ciphertexts[i]));
      EXPECT_EQ(plaintexts[i], decrypted);
    }
  }

  std::unique_ptr<uint_m::Params> params14_;
  NttParameters<uint_m> ntt_params_;
  std::unique_ptr<ErrorParams<uint_m>> error_params_;
};

// Ensure that the encryption scheme can encrypt and decrypt a single compressed
// ciphertext.
TEST_F(SymmetricEncryptionWithPrngTest, EncryptDecryptSingleCompressed) {
  for (unsigned int i = 0; i < kTestingRounds; ++i) {
    TestCompressedEncryptionDecryption(
        {rlwe::testing::SamplePlaintext<uint_m>()});
  }
}

// Ensure that the encryption scheme can encrypt and decrypt multiple compressed
// ciphertexts.
TEST_F(SymmetricEncryptionWithPrngTest, EncryptDecryptMultipleCompressed) {
  for (unsigned int i = 0; i < kTestingRounds; ++i) {
    std::vector<std::vector<uint_m::Int>> plaintexts;
    for (int j = 0; j < i + 2; ++j) {
      plaintexts.push_back(rlwe::testing::SamplePlaintext<uint_m>());
    }
    TestCompressedEncryptionDecryption(plaintexts);
  }
}

}  // namespace
}  // namespace rlwe
