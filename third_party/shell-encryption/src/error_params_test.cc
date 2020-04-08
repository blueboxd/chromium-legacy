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

#ifndef RLWE_ERROR_PARAMS_TEST_H_
#define RLWE_ERROR_PARAMS_TEST_H_

#include "error_params.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "prng/integral_prng_types.h"
#include "status_macros.h"
#include "symmetric_encryption.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"

namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

// Number of samples used to compute the actual variance.
const rlwe::Uint64 kSamples = 50;

using uint_m = rlwe::MontgomeryInt<rlwe::Uint32>;
using Polynomial = rlwe::Polynomial<uint_m>;
using Ciphertext = rlwe::SymmetricRlweCiphertext<uint_m>;
using Key = rlwe::SymmetricRlweKey<uint_m>;

class ErrorParamsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params14_,
                         rlwe::testing::ConstructMontgomeryIntParams());
    ASSERT_OK_AND_ASSIGN(ntt_params_,
                         rlwe::InitializeNttParameters<uint_m>(
                             rlwe::testing::kLogCoeffs, params14_.get()));
    ASSERT_OK_AND_ASSIGN(auto error_params, rlwe::ErrorParams<uint_m>::Create(
                                                rlwe::testing::kDefaultLogT,
                                                rlwe::testing::kDefaultVariance,
                                                params14_.get(), &ntt_params_));
    error_params_ = absl::make_unique<rlwe::ErrorParams<uint_m>>(error_params);
  }

  // Computes the l-infinity norm of a vector of Ints.
  uint_m::Int ComputeNorm(std::vector<uint_m::Int> coeffs) {
    return *std::max_element(coeffs.begin(), coeffs.end());
  }

  // Convert a vector of integers to a vector of montgomery integers.
  rlwe::StatusOr<std::vector<uint_m>> ConvertToMontgomery(
      const std::vector<uint_m::Int>& coeffs) {
    return rlwe::testing::ConvertToMontgomery<uint_m>(coeffs, params14_.get());
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(
      uint_m::Int variance = rlwe::testing::kDefaultVariance,
      uint_m::Int log_t = rlwe::testing::kDefaultLogT) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(rlwe::testing::kLogCoeffs, variance, log_t,
                       params14_.get(), &ntt_params_, prng.get());
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<uint_m::Int>& plaintext) {
    RLWE_ASSIGN_OR_RETURN(auto m_p, ConvertToMontgomery(plaintext));
    auto plaintext_ntt =
        Polynomial::ConvertToNtt(m_p, ntt_params_, params14_.get());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<uint_m>(key, plaintext_ntt, error_params_.get(),
                                 prng.get());
  }

  // Decrypt without removing the error, returning (m + et).
  rlwe::StatusOr<std::vector<uint_m::Int>> GetErrorAndMessage(
      const Key& key, const Ciphertext& ciphertext) {
    Polynomial error_and_message_ntt(key.Len(), key.ModulusParams());
    Polynomial key_powers = key.Key();
    for (int i = 0; i < ciphertext.Len(); i++) {
      // Extract component i.
      RLWE_ASSIGN_OR_RETURN(Polynomial ci, ciphertext.Component(i));

      if (i > 1) {
        RLWE_RETURN_IF_ERROR(
            key_powers.MulInPlace(key.Key(), key.ModulusParams()));
      }
      // Beyond c0, multiply the exponentiated key in.
      if (i > 0) {
        RLWE_RETURN_IF_ERROR(ci.MulInPlace(key_powers, key.ModulusParams()));
      }
      RLWE_RETURN_IF_ERROR(
          error_and_message_ntt.AddInPlace(ci, key.ModulusParams()));
    }
    auto error_and_message = error_and_message_ntt.InverseNtt(
        *(key.NttParams()), key.ModulusParams());

    // Convert the integers mod q to integers.
    std::vector<uint_m::Int> error_and_message_ints(error_and_message.size(),
                                                    0);
    for (int i = 0; i < error_and_message.size(); i++) {
      error_and_message_ints[i] =
          error_and_message[i].ExportInt(key.ModulusParams());
      if (error_and_message_ints[i] > (key.ModulusParams()->modulus >> 1)) {
        error_and_message_ints[i] =
            key.ModulusParams()->modulus - error_and_message_ints[i];
      }
    }
    return error_and_message_ints;
  }

  // Modulus params.
  std::unique_ptr<uint_m::Params> params14_;
  // NTT params.
  rlwe::NttParameters<uint_m> ntt_params_;
  // Error params.
  std::unique_ptr<rlwe::ErrorParams<uint_m>> error_params_;
};

TEST_F(ErrorParamsTest, CreateError) {
  // large value for log_t
  const int log_t = params14_->log_modulus;
  EXPECT_THAT(
      rlwe::ErrorParams<uint_m>::Create(log_t, rlwe::testing::kDefaultVariance,
                                        params14_.get(), &ntt_params_),
      StatusIs(
          ::absl::StatusCode::kInvalidArgument,
          HasSubstr(absl::StrCat("The value log_t, ", log_t,
                                 ", must be smaller than log_modulus - 1, ",
                                 log_t - 1, "."))));
}

TEST_F(ErrorParamsTest, PlaintextError) {
  // Randomly sample polynomials and expect l-infinity norm is bounded by
  // b_plaintext.
  for (int i = 0; i < kSamples; i++) {
    // Samples a polynomial with kLogT and kDefaultCoeffs.
    auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

    // Expect that the norm of the coefficients of the plaintext is less than
    // b_plaintext.
    uint_m::Int norm = ComputeNorm(plaintext);
    EXPECT_LT(norm, error_params_->B_plaintext());
  }
}

TEST_F(ErrorParamsTest, EncryptionError) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  // Randomly sample polynomials, decrypt, and compute the size of the result
  // before removing error.
  for (int i = 0; i < kSamples; i++) {
    // Expect that the norm of the coefficients of (m + et) is less than
    // b_encryption.
    auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
    ASSERT_OK_AND_ASSIGN(auto error_and_message,
                         GetErrorAndMessage(key, ciphertext));

    EXPECT_LT(ComputeNorm(error_and_message), error_params_->B_encryption());
  }
}

TEST_F(ErrorParamsTest, RelinearizationErrorScalesWithT) {
  // Error scales by (T / logT) when all other constants are fixed.
  int small_decomposition_modulus = 1;
  int large_decomposition_modulus = 10;
  EXPECT_LT(error_params_->B_relinearize(small_decomposition_modulus),
            error_params_->B_relinearize(large_decomposition_modulus));
}

}  //  namespace

#endif  // RLWE_ERROR_PARAMS_TEST_H_
