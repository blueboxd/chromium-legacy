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

#include "symmetric_encryption.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "constants.h"
#include "montgomery.h"
#include "ntt_parameters.h"
#include "polynomial.h"
#include "prng/integral_prng_types.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"
#include "testing/testing_utils.h"


namespace {

using ::rlwe::testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

// Set constants.
const int kTestingRounds = 10;

// Useful typedefs.
using uint_m = rlwe::MontgomeryInt<rlwe::Uint32>;
using Polynomial = rlwe::Polynomial<uint_m>;
using Ciphertext = rlwe::SymmetricRlweCiphertext<uint_m>;
using Key = rlwe::SymmetricRlweKey<uint_m>;
using ErrorParams = rlwe::ErrorParams<uint_m>;

// Tests symmetric-key encryption scheme, including the following homomorphic
// operations: addition, scalar multiplication by a polynomial (absorb), and
// multiplication. Substituions are implemented in polynomial_ciphertext.h, and
// SymmetricRlweKey::Substitute and SymmetricRlweCiphertext::PowersOfS()
// (updated on substitution calls) are further tested in
// polynomial_ciphertext_test.cc.
class SymmetricRlweEncryptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(params_,
                         rlwe::testing::ConstructMontgomeryIntParams());
    ASSERT_OK_AND_ASSIGN(ntt_params_,
                         rlwe::InitializeNttParameters<uint_m>(
                             rlwe::testing::kLogCoeffs, params_.get()));
    ASSERT_OK_AND_ASSIGN(
        auto temp_error_params,
        rlwe::ErrorParams<uint_m>::Create(rlwe::testing::kDefaultLogT,
                                          rlwe::testing::kDefaultVariance,
                                          params_.get(), &ntt_params_));
    error_params_ = absl::make_unique<ErrorParams>(temp_error_params);
  }

  // Sample a random key.
  rlwe::StatusOr<Key> SampleKey(
      uint_m::Int variance = rlwe::testing::kDefaultVariance,
      uint_m::Int log_t = rlwe::testing::kDefaultLogT) {
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return Key::Sample(rlwe::testing::kLogCoeffs, variance, log_t,
                       params_.get(), &ntt_params_, prng.get());
  }

  // Encrypt a plaintext.
  rlwe::StatusOr<Ciphertext> Encrypt(
      const Key& key, const std::vector<uint_m::Int>& plaintext) {
    RLWE_ASSIGN_OR_RETURN(auto mont, rlwe::testing::ConvertToMontgomery<uint_m>(
                                         plaintext, params_.get()));
    auto plaintext_ntt =
        Polynomial::ConvertToNtt(mont, ntt_params_, params_.get());
    RLWE_ASSIGN_OR_RETURN(std::string prng_seed,
                          rlwe::SingleThreadPrng::GenerateSeed());
    RLWE_ASSIGN_OR_RETURN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    return rlwe::Encrypt<uint_m>(key, plaintext_ntt, error_params_.get(),
                                 prng.get());
  }

  std::unique_ptr<uint_m::Params> params_;
  rlwe::NttParameters<uint_m> ntt_params_;
  std::unique_ptr<ErrorParams> error_params_;
};

// Ensure that RemoveError works correctly on negative numbers for several
// different values of t.
TEST_F(SymmetricRlweEncryptionTest, RemoveErrorNegative) {
  unsigned int seed = 0;

  for (int t = 2; t < 16; t++) {
    for (int i = 0; i < kTestingRounds; i++) {
      // Sample a plaintext in the range (modulus/2, modulus)
      uint_m::Int plaintext = (rand_r(&seed) % (rlwe::testing::kModulus / 2)) +
                              rlwe::testing::kModulus / 2 + 1;
      // Create a vector that exclusively contains the value "plaintext".
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           uint_m::ImportInt(plaintext, params_.get()));
      std::vector<uint_m> error_and_message(rlwe::testing::kCoeffs,
                                            m_plaintext);
      auto result = rlwe::RemoveError<uint_m>(
          error_and_message, params_->modulus, t, params_.get());

      // Compute the expected result using signed arithmetic. Derive its
      // negative equivalent by subtracting out testing::kModulus and taking
      // that negative value (mod t).
      int64_t expected = (static_cast<int64_t>(plaintext) -
                          static_cast<int64_t>(rlwe::testing::kModulus)) %
                         t;

      // Finally, turn any negative values into their positive equivalents
      // (mod t).
      if (expected < 0) {
        expected += t;
      }

      for (unsigned int j = 0; j < rlwe::testing::kCoeffs; j++) {
        EXPECT_EQ(expected, result[j]) << t << plaintext;
      }
    }
  }
}

// Ensure that RemoveError works correctly on positive numbers for several
// different values of t.
TEST_F(SymmetricRlweEncryptionTest, RemoveErrorPositive) {
  unsigned int seed = 0;

  for (int t = 2; t < 16; t++) {
    for (int i = 0; i < kTestingRounds; i++) {
      // Sample a plaintext in the range (0, modulus/2)
      uint_m::Int plaintext = rand_r(&seed) % (rlwe::testing::kModulus / 2);

      // Create a vector that exclusively contains the value "plaintext".
      ASSERT_OK_AND_ASSIGN(auto m_plaintext,
                           uint_m::ImportInt(plaintext, params_.get()));
      std::vector<uint_m> error_and_message(rlwe::testing::kCoeffs,
                                            m_plaintext);
      auto result = rlwe::RemoveError<uint_m>(
          error_and_message, params_->modulus, t, params_.get());

      for (unsigned int j = 0; j < rlwe::testing::kCoeffs; j++) {
        EXPECT_EQ(plaintext % t, result[j]);
      }
    }
  }
}

// Ensure that the encryption scheme can decrypt its own ciphertexts.
TEST_F(SymmetricRlweEncryptionTest, CanDecrypt) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
    auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
    ASSERT_OK_AND_ASSIGN(auto decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(plaintext, decrypted);
  }
}

// Accessing out of bounds raises errors
TEST_F(SymmetricRlweEncryptionTest, OutOfBoundsIndex) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
  ASSERT_OK(ciphertext.Component(ciphertext.Len() - 1));
  EXPECT_THAT(ciphertext.Component(ciphertext.Len()),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("Index out of range.")));
  EXPECT_THAT(ciphertext.Component(-1),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("Index out of range.")));
}

TEST_F(SymmetricRlweEncryptionTest, AdditivelyHomomorphic) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    std::vector<uint_m::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<uint_m>();
    std::vector<uint_m::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<uint_m>();

    ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));
    ASSERT_OK_AND_ASSIGN(auto ciphertext_add, ciphertext1 + ciphertext2);
    ASSERT_OK_AND_ASSIGN(auto ciphertext_sub, ciphertext1 - ciphertext2);

    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted_add,
                         rlwe::Decrypt<uint_m>(key, ciphertext_add));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted_sub,
                         rlwe::Decrypt<uint_m>(key, ciphertext_sub));

    for (unsigned int j = 0; j < plaintext1.size(); j++) {
      EXPECT_EQ((plaintext1[j] + plaintext2[j]) % rlwe::testing::kDefaultT,
                decrypted_add[j]);
      EXPECT_EQ((rlwe::testing::kDefaultT + plaintext1[j] - plaintext2[j]) %
                    rlwe::testing::kDefaultT,
                decrypted_sub[j]);
      // Check that the error grows additively.
      EXPECT_EQ(ciphertext_add.Error(),
                ciphertext1.Error() + ciphertext2.Error());
      EXPECT_EQ(ciphertext_sub.Error(),
                ciphertext1.Error() + ciphertext2.Error());
    }
  }
}

TEST_F(SymmetricRlweEncryptionTest, AddHomomorphicallyInPlace) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    std::vector<uint_m::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<uint_m>();
    std::vector<uint_m::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<uint_m>();

    ASSERT_OK_AND_ASSIGN(auto ciphertext1_add, Encrypt(key, plaintext1));
    ASSERT_OK_AND_ASSIGN(auto ciphertext1_sub, Encrypt(key, plaintext1));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));
    const double ciphertext1_add_error = ciphertext1_add.Error();
    const double ciphertext1_sub_error = ciphertext1_sub.Error();

    ASSERT_OK(ciphertext1_add.AddInPlace(ciphertext2));
    ASSERT_OK(ciphertext1_sub.SubInPlace(ciphertext2));

    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted1_add,
                         rlwe::Decrypt<uint_m>(key, ciphertext1_add));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted1_sub,
                         rlwe::Decrypt<uint_m>(key, ciphertext1_sub));

    for (unsigned int j = 0; j < plaintext1.size(); j++) {
      EXPECT_EQ((plaintext1[j] + plaintext2[j]) % rlwe::testing::kDefaultT,
                decrypted1_add[j]);
      EXPECT_EQ((rlwe::testing::kDefaultT + plaintext1[j] - plaintext2[j]) %
                    rlwe::testing::kDefaultT,
                decrypted1_sub[j]);
      // Check that the error grows additively.
      EXPECT_EQ(ciphertext1_add.Error(),
                ciphertext1_add_error + ciphertext2.Error());
      EXPECT_EQ(ciphertext1_sub.Error(),
                ciphertext1_sub_error + ciphertext2.Error());
    }
  }
}

TEST_F(SymmetricRlweEncryptionTest, AddToZero) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();

    Ciphertext ciphertext1(params_.get(), error_params_.get());
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext));
    ASSERT_OK_AND_ASSIGN(auto ciphertext3, ciphertext1 + ciphertext2);

    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext3));

    EXPECT_EQ(plaintext, decrypted);
  }
}

TEST_F(SymmetricRlweEncryptionTest, Absorb) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    // Create the initial plaintexts.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_plaintext,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));
    Polynomial plaintext_ntt =
        Polynomial::ConvertToNtt(m_plaintext, ntt_params_, params_.get());
    std::vector<uint_m::Int> to_absorb =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_to_absorb,
        rlwe::testing::ConvertToMontgomery<uint_m>(to_absorb, params_.get()));
    Polynomial to_absorb_ntt =
        Polynomial::ConvertToNtt(m_to_absorb, ntt_params_, params_.get());

    // Create our expected value.
    ASSERT_OK_AND_ASSIGN(Polynomial expected_ntt,
                         plaintext_ntt.Mul(to_absorb_ntt, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    // Encrypt, absorb, and decrypt.
    ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
    auto ciphertext = encrypt * to_absorb_ntt;
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(expected, decrypted);

    // Check that the error is the product of an encryption and a plaintext.
    EXPECT_EQ(ciphertext.Error(),
              error_params_->B_encryption() * error_params_->B_plaintext());
  }
}

TEST_F(SymmetricRlweEncryptionTest, AbsorbInPlace) {
  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    // Create the initial plaintexts.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_plaintext,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));
    Polynomial plaintext_ntt =
        Polynomial::ConvertToNtt(m_plaintext, ntt_params_, params_.get());
    std::vector<uint_m::Int> to_absorb =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_to_absorb,
        rlwe::testing::ConvertToMontgomery<uint_m>(to_absorb, params_.get()));
    Polynomial to_absorb_ntt =
        Polynomial::ConvertToNtt(m_to_absorb, ntt_params_, params_.get());

    // Create our expected value.
    ASSERT_OK_AND_ASSIGN(Polynomial expected_ntt,
                         plaintext_ntt.Mul(to_absorb_ntt, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    // Encrypt, absorb in place, and decrypt.
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
    ciphertext.AbsorbInPlace(to_absorb_ntt);
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(expected, decrypted);

    // Check that the error is the product of an encryption and a plaintext.
    EXPECT_EQ(ciphertext.Error(),
              error_params_->B_encryption() * error_params_->B_plaintext());
  }
}

TEST_F(SymmetricRlweEncryptionTest, AbsorbScalar) {
  unsigned int seed = 0;

  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    // Create the initial plaintexts.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_plaintext,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));
    Polynomial plaintext_ntt =
        Polynomial::ConvertToNtt(m_plaintext, ntt_params_, params_.get());
    ASSERT_OK_AND_ASSIGN(
        uint_m to_absorb,
        uint_m::ImportInt(rand_r(&seed) % rlwe::testing::kDefaultT,
                          params_.get()));

    // Create our expected value.
    ASSERT_OK_AND_ASSIGN(Polynomial expected_ntt,
                         plaintext_ntt.Mul(to_absorb, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    // Encrypt, absorb, and decrypt.
    ASSERT_OK_AND_ASSIGN(auto encrypt, Encrypt(key, plaintext));
    auto ciphertext = encrypt * to_absorb;
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(expected, decrypted);
    // Expect the error to grow multiplicatively.
    EXPECT_EQ(ciphertext.Error(), error_params_->B_encryption() *
                                      to_absorb.ExportInt(params_.get()));
  }
}

TEST_F(SymmetricRlweEncryptionTest, AbsorbScalarInPlace) {
  unsigned int seed = 0;

  for (unsigned int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

    // Create the initial plaintexts.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto m_plaintext,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));
    Polynomial plaintext_ntt =
        Polynomial::ConvertToNtt(m_plaintext, ntt_params_, params_.get());
    ASSERT_OK_AND_ASSIGN(
        uint_m to_absorb,
        uint_m::ImportInt(rand_r(&seed) % rlwe::testing::kDefaultT,
                          params_.get()));

    // Create our expected value.
    ASSERT_OK_AND_ASSIGN(Polynomial expected_ntt,
                         plaintext_ntt.Mul(to_absorb, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    // Encrypt, absorb, and decrypt.
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
    ciphertext.AbsorbInPlace(to_absorb);
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(expected, decrypted);
    // Expect the error to grow multiplicatively.
    EXPECT_EQ(ciphertext.Error(), error_params_->B_encryption() *
                                      to_absorb.ExportInt(params_.get()));
  }
}

TEST_F(SymmetricRlweEncryptionTest, EmptyCipherMultiplication) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  // Create a plaintext
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  // Encrypt, multiply
  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext));

  // empty cipher
  std::vector<Polynomial> c;
  Ciphertext ciphertext2(c, 1, 0, params_.get(), error_params_.get());

  EXPECT_THAT(
      ciphertext1 * ciphertext2,
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("Cannot multiply using an empty ciphertext.")));
  EXPECT_THAT(
      ciphertext2 * ciphertext1,
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("Cannot multiply using an empty ciphertext.")));

  c.push_back(Polynomial());
  Ciphertext ciphertext3(c, 1, 0, params_.get(), error_params_.get());
  EXPECT_THAT(
      ciphertext1 * ciphertext3,
      StatusIs(
          ::absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Cannot multiply using an empty polynomial in the ciphertext.")));

  EXPECT_THAT(
      ciphertext3 * ciphertext1,
      StatusIs(
          ::absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Cannot multiply using an empty polynomial in the ciphertext.")));
}

TEST_F(SymmetricRlweEncryptionTest, MultiplicativelyHomomorphic) {
  for (int i = 0; i < kTestingRounds; i++) {
    // Since error builds up *very* quickly for homomorphic encryption, use
    // an alternate value for the variance.
    uint_m::Int variance = 4;

    ASSERT_OK_AND_ASSIGN(auto key, SampleKey(variance));

    // Create the initial plaintexts.
    std::vector<uint_m::Int> plaintext1 =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(auto mp1, rlwe::testing::ConvertToMontgomery<uint_m>(
                                       plaintext1, params_.get()));
    Polynomial plaintext1_ntt =
        Polynomial::ConvertToNtt(mp1, ntt_params_, params_.get());
    std::vector<uint_m::Int> plaintext2 =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(auto mp2, rlwe::testing::ConvertToMontgomery<uint_m>(
                                       plaintext2, params_.get()));
    Polynomial plaintext2_ntt =
        Polynomial::ConvertToNtt(mp2, ntt_params_, params_.get());

    // Encrypt, multiply, and decrypt.
    ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
    ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));
    ASSERT_OK_AND_ASSIGN(auto product, ciphertext1* ciphertext2);
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key, product));

    // Create the polynomial we expect.
    ASSERT_OK_AND_ASSIGN(Polynomial expected_ntt,
                         plaintext1_ntt.Mul(plaintext2_ntt, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    EXPECT_EQ(expected, decrypted);
    // Expect that the error grows multiplicatively.
    EXPECT_EQ(product.Error(), ciphertext1.Error() * ciphertext2.Error());
  }
}

TEST_F(SymmetricRlweEncryptionTest, ManyHomomorphicAdds) {
  // Sample a starting plaintext and ciphertext and create aggregators;
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext_sum = plaintext;
  ASSERT_OK_AND_ASSIGN(Key key, SampleKey());
  ASSERT_OK_AND_ASSIGN(auto ciphertext_sum, Encrypt(key, plaintext));

  // Sample a fresh plaintext.
  plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

  int num_adds = 50;
  // Perform 50 homomorphic ciphertext additions with the fresh ciphertext.
  for (int j = 0; j < num_adds; j++) {
    // Add the new plaintext to the old plaintext.
    for (unsigned int k = 0; k < rlwe::testing::kCoeffs; k++) {
      plaintext_sum[k] += plaintext[k];
      plaintext_sum[k] %= rlwe::testing::kDefaultT;
    }

    // Add the new ciphertext to the old ciphertext.
    ASSERT_OK_AND_ASSIGN(ciphertext_sum, ciphertext_sum + ciphertext);
  }

  ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                       rlwe::Decrypt<uint_m>(key, ciphertext_sum));

  // Ensure the values are the same.
  EXPECT_EQ(plaintext_sum, decrypted);
  // Expect that the ciphertext sum's error grows by the additively by the
  // ciphertext's error.
  EXPECT_GT(ciphertext_sum.Error(), num_adds * ciphertext.Error());
}

TEST_F(SymmetricRlweEncryptionTest, ExceedMaxNumCoeffDeserializeCiphertext) {
  int num_coeffs = rlwe::kMaxNumCoeffs + 1;
  std::vector<Polynomial> c;
  for (int i = 0; i < num_coeffs; i++) {
    c.push_back(Polynomial(1, params_.get()));
  }
  Ciphertext ciphertext(c, 1, 0, params_.get(), error_params_.get());
  // Serialize and deserialize.
  ASSERT_OK_AND_ASSIGN(rlwe::SerializedSymmetricRlweCiphertext serialized,
                       ciphertext.Serialize());

  EXPECT_THAT(
      Ciphertext::Deserialize(serialized, params_.get(), error_params_.get()),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr(absl::StrCat(
                   "Number of coefficients, ", serialized.c_size(),
                   ", cannot be more than ", rlwe::kMaxNumCoeffs, "."))));
}

TEST_F(SymmetricRlweEncryptionTest, SerializeCiphertext) {
  for (int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(Key key, SampleKey());
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

    // Serialize and deserialize.
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedSymmetricRlweCiphertext serialized,
                         ciphertext.Serialize());
    ASSERT_OK_AND_ASSIGN(auto deserialized,
                         Ciphertext::Deserialize(serialized, params_.get(),
                                                 error_params_.get()));

    // Decrypt and check equality.
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> deserialized_plaintext,
                         rlwe::Decrypt<uint_m>(key, deserialized));

    EXPECT_EQ(plaintext, deserialized_plaintext);
    // Check that the error stays the same.
    EXPECT_EQ(deserialized.Error(), ciphertext.Error());
  }
}

TEST_F(SymmetricRlweEncryptionTest, SerializeKey) {
  for (int i = 0; i < kTestingRounds; i++) {
    ASSERT_OK_AND_ASSIGN(Key original_key, SampleKey());
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();

    // Serialize key, deserialize, and ensure the deserialized key is
    // interoperable with the original key.
    ASSERT_OK_AND_ASSIGN(rlwe::SerializedNttPolynomial serialized,
                         original_key.Serialize());
    ASSERT_OK_AND_ASSIGN(
        Key deserialized_key,
        Key::Deserialize(rlwe::testing::kDefaultVariance,
                         rlwe::testing::kDefaultLogT, serialized, params_.get(),
                         &ntt_params_));

    // Test that a ciphertext encrypted with the original key decrypts under the
    // deserialized key.
    ASSERT_OK_AND_ASSIGN(auto ekey1, Encrypt(original_key, plaintext));
    ASSERT_OK_AND_ASSIGN(auto dkey1,
                         rlwe::Decrypt<uint_m>(deserialized_key, ekey1));
    EXPECT_EQ(dkey1, plaintext);

    // Test that a ciphertext encrypted with the deserialized key decrypts under
    // the original key.
    ASSERT_OK_AND_ASSIGN(auto ekey2, Encrypt(deserialized_key, plaintext));
    ASSERT_OK_AND_ASSIGN(auto dkey2,
                         rlwe::Decrypt<uint_m>(original_key, ekey2));
    EXPECT_EQ(dkey2, plaintext);
  }
}

// Try an ill-formed key modulus switching
TEST_F(SymmetricRlweEncryptionTest, FailingKeyModulusReduction) {
  // p is the original modulus and q is the new modulus we want to switch to
  // t is the bound for the coefficients of the message for decryption to work
  // typically, p % t must be equal to q % t and both p, q are 1 mod 2*n
  // for failure, we need to find q such that p % t != q % t and q = 1 mod 2*n
  // let t = 2*n + 1, and q = p + 2*n, and we get both properties
  const rlwe::Uint64 p = rlwe::testing::kModulus;
  const rlwe::Uint64 two_times_n = 1 << (rlwe::testing::kLogCoeffs + 1);
  const rlwe::Uint64 q = p + two_times_n;

  ASSERT_OK_AND_ASSIGN(auto params_q, uint_m::Params::Create(q));
  ASSERT_OK_AND_ASSIGN(auto ntt_params_q,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params_q.get()));
  // arguments to sample key
  // rlwe::testing::kLogCoeffs, variance, log_t, mod_params, ntt_params
  // since t = 2*n + 1, we just set the argument to log(2n) = log(n) + 1
  // (the "+ 1" is handled by the function itself)
  ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                       rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
  ASSERT_OK_AND_ASSIGN(
      auto key_q,
      rlwe::SymmetricRlweKey<uint_m>::Sample(
          rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
          rlwe::testing::kLogCoeffs + 1, params_q.get(), &ntt_params_q,
          prng.get()));

  EXPECT_THAT(key_q.SwitchModulus<uint_m>(params_.get(), &ntt_params_),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("p % t != q % t")));
}

// Try an ill-formed ciphertext modulus switching
TEST_F(SymmetricRlweEncryptionTest, FailingCiphertextModulusReduction) {
  // sample ciphertext under modulus p
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));

  // p is the original modulus and q is the new modulus we want to switch to
  // t is the bound for the coefficients of the message for decryption to work
  // typically, p % t must be equal to q % t and both p, q are 1 mod 2*n
  // for failure, we need to find q such that p % t != q % t and q = 1 mod 2*n
  // let t = 2*n + 1, and q = p - 2*n, and we get both properties
  rlwe::Uint64 p = rlwe::testing::kModulus;
  rlwe::Uint64 two_times_n = 1 << (rlwe::testing::kLogCoeffs + 1);
  rlwe::Uint64 q = p - two_times_n;

  // create params under mod q
  auto params_q = uint_m::Params::Create(q).ValueOrDie();
  ASSERT_OK_AND_ASSIGN(auto ntt_params_q,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params_q.get()));
  ASSERT_OK_AND_ASSIGN(auto error_params_q,
                       ErrorParams::Create(rlwe::testing::kLogCoeffs,
                                           rlwe::testing::kDefaultVariance,
                                           params_q.get(), &ntt_params_q));
  auto error_params_q_ = absl::make_unique<ErrorParams>(error_params_q);

  EXPECT_THAT(ciphertext.SwitchModulus<uint_m>(
                  &ntt_params_, params_q.get(), &ntt_params_q,
                  error_params_q_.get(), two_times_n + 1),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("p % t != q % t")));
}

// Test modulus switching.
TEST_F(SymmetricRlweEncryptionTest, ModulusReduction) {
  // 29-bit modulus.
  ASSERT_OK_AND_ASSIGN(auto params29, uint_m::Params::Create(rlwe::kModulus29));
  ASSERT_OK_AND_ASSIGN(auto ntt_params29,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params29.get()));
  ASSERT_OK_AND_ASSIGN(auto error_params29,
                       ErrorParams::Create(rlwe::testing::kDefaultLogT,
                                           rlwe::testing::kDefaultVariance,
                                           params29.get(), &ntt_params29));

  for (int i = 0; i < kTestingRounds; i++) {
    // Create a key.
    ASSERT_OK_AND_ASSIGN(std::string sample_prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto sample_prng,
                         rlwe::SingleThreadPrng::Create(sample_prng_seed));
    ASSERT_OK_AND_ASSIGN(
        auto key29,
        rlwe::SymmetricRlweKey<uint_m>::Sample(
            rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
            rlwe::testing::kDefaultLogT, params29.get(), &ntt_params29,
            sample_prng.get()));
    ASSERT_OK_AND_ASSIGN(
        auto key, key29.SwitchModulus<uint_m>(params_.get(), &ntt_params_));

    // Create a plaintext.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto plaintext_montgomery,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params29.get()));

    // Encrypt.
    auto plaintext_ntt = rlwe::Polynomial<uint_m>::ConvertToNtt(
        plaintext_montgomery, ntt_params29, params29.get());
    ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(auto ciphertext29,
                         rlwe::Encrypt<uint_m>(key29, plaintext_ntt,
                                               &error_params29, prng.get()));

    // Switch moduli.
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         ciphertext29.SwitchModulus<uint_m>(
                             &ntt_params29, params_.get(), &ntt_params_,
                             error_params_.get(), rlwe::testing::kDefaultT));

    // Decrypt in the smaller modulus.
    ASSERT_OK_AND_ASSIGN(auto decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(plaintext, decrypted);
  }
}

TEST_F(SymmetricRlweEncryptionTest, ModulusReductionOnlyKeySwitch) {
  ASSERT_OK_AND_ASSIGN(auto params29, uint_m::Params::Create(rlwe::kModulus29));
  ASSERT_OK_AND_ASSIGN(auto ntt_params29,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params29.get()));

  for (int i = 0; i < kTestingRounds; i++) {
    // Create a key.
    ASSERT_OK_AND_ASSIGN(std::string sample_prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto sample_prng,
                         rlwe::SingleThreadPrng::Create(sample_prng_seed));
    ASSERT_OK_AND_ASSIGN(
        auto key29,
        rlwe::SymmetricRlweKey<uint_m>::Sample(
            rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
            rlwe::testing::kDefaultLogT, params29.get(), &ntt_params29,
            sample_prng.get()));
    ASSERT_OK_AND_ASSIGN(
        auto key, key29.SwitchModulus<uint_m>(params_.get(), &ntt_params_));

    // Create a plaintext.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto plaintext_montgomery,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));

    // Encrypt.
    auto plaintext_ntt = rlwe::Polynomial<uint_m>::ConvertToNtt(
        plaintext_montgomery, ntt_params_, params_.get());
    ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(auto ciphertext, rlwe::Encrypt<uint_m>(
                                              key, plaintext_ntt,
                                              error_params_.get(), prng.get()));

    ASSERT_OK_AND_ASSIGN(auto decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    EXPECT_EQ(plaintext, decrypted);
  }
}

TEST_F(SymmetricRlweEncryptionTest, SerializeModulusSwitchedKey) {
  // 29-bit modulus.
  ASSERT_OK_AND_ASSIGN(auto params29, uint_m::Params::Create(rlwe::kModulus29));
  ASSERT_OK_AND_ASSIGN(auto ntt_params29,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params29.get()));
  ASSERT_OK_AND_ASSIGN(auto error_params29,
                       ErrorParams::Create(rlwe::testing::kDefaultLogT,
                                           rlwe::testing::kDefaultVariance,
                                           params29.get(), &ntt_params29));

  for (int i = 0; i < kTestingRounds; i++) {
    // Create a key.
    ASSERT_OK_AND_ASSIGN(std::string sample_prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto sample_prng,
                         rlwe::SingleThreadPrng::Create(sample_prng_seed));
    ASSERT_OK_AND_ASSIGN(
        auto key29,
        rlwe::SymmetricRlweKey<uint_m>::Sample(
            rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
            rlwe::testing::kDefaultLogT, params29.get(), &ntt_params29,
            sample_prng.get()));
    ASSERT_OK_AND_ASSIGN(
        auto key, key29.SwitchModulus<uint_m>(params_.get(), &ntt_params_));

    // Create a plaintext.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto plaintext_montgomery,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params29.get()));

    // Encrypt.
    auto plaintext_ntt = rlwe::Polynomial<uint_m>::ConvertToNtt(
        plaintext_montgomery, ntt_params29, params29.get());
    ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(auto ciphertext29,
                         rlwe::Encrypt<uint_m>(key29, plaintext_ntt,
                                               &error_params29, prng.get()));

    // Serialize and deserialize key.
    ASSERT_OK_AND_ASSIGN(auto serialized_key, key.Serialize());
    ASSERT_OK_AND_ASSIGN(
        auto deserialized_key,
        rlwe::SymmetricRlweKey<uint_m>::Deserialize(
            rlwe::testing::kDefaultVariance, rlwe::testing::kDefaultLogT,
            serialized_key, params_.get(), params29.get(), &ntt_params_));

    // Switch moduli.
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         ciphertext29.SwitchModulus<uint_m>(
                             &ntt_params29, params_.get(), &ntt_params_,
                             error_params_.get(), rlwe::testing::kDefaultT));

    // Decrypt in the smaller modulus.
    ASSERT_OK_AND_ASSIGN(auto decrypted,
                         rlwe::Decrypt<uint_m>(deserialized_key, ciphertext));

    EXPECT_EQ(plaintext, decrypted);
  }
}

TEST_F(SymmetricRlweEncryptionTest, ModulusSwitchingReducesLargeError) {
  // 29-bit modulus.
  ASSERT_OK_AND_ASSIGN(auto params29, uint_m::Params::Create(rlwe::kModulus29));
  ASSERT_OK_AND_ASSIGN(auto ntt_params29,
                       rlwe::InitializeNttParameters<uint_m>(
                           rlwe::testing::kLogCoeffs, params29.get()));
  ASSERT_OK_AND_ASSIGN(auto error_params29,
                       ErrorParams::Create(rlwe::testing::kDefaultLogT,
                                           rlwe::testing::kDefaultVariance,
                                           params29.get(), &ntt_params29));

  for (int i = 0; i < kTestingRounds; i++) {
    // Create a key.
    ASSERT_OK_AND_ASSIGN(std::string sample_prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto sample_prng,
                         rlwe::SingleThreadPrng::Create(sample_prng_seed));
    ASSERT_OK_AND_ASSIGN(
        auto key29,
        rlwe::SymmetricRlweKey<uint_m>::Sample(
            rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
            rlwe::testing::kDefaultLogT, params29.get(), &ntt_params29,
            sample_prng.get()));
    ASSERT_OK_AND_ASSIGN(
        auto key, key29.SwitchModulus<uint_m>(params_.get(), &ntt_params_));

    // Create a plaintext.
    std::vector<uint_m::Int> plaintext =
        rlwe::testing::SamplePlaintext<uint_m>();
    ASSERT_OK_AND_ASSIGN(
        auto plaintext_montgomery,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params29.get()));

    // Encrypt and square ciphertext.
    auto plaintext_ntt = rlwe::Polynomial<uint_m>::ConvertToNtt(
        plaintext_montgomery, ntt_params29, params29.get());
    ASSERT_OK_AND_ASSIGN(std::string prng_seed,
                         rlwe::SingleThreadPrng::GenerateSeed());
    ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
    ASSERT_OK_AND_ASSIGN(auto ciphertext29,
                         rlwe::Encrypt<uint_m>(key29, plaintext_ntt,
                                               &error_params29, prng.get()));
    ASSERT_OK_AND_ASSIGN(auto squared29, ciphertext29* ciphertext29);

    // Switch moduli.
    ASSERT_OK_AND_ASSIGN(auto ciphertext,
                         squared29.SwitchModulus<uint_m>(
                             &ntt_params29, params_.get(), &ntt_params_,
                             error_params_.get(), rlwe::testing::kDefaultT));

    // Decrypt in the smaller modulus.
    ASSERT_OK_AND_ASSIGN(auto decrypted,
                         rlwe::Decrypt<uint_m>(key, ciphertext));

    // Expect that the error reduces after a modulus switch when the error is
    // large.
    EXPECT_LT(ciphertext.Error(), squared29.Error());
    // But that the error doesn't reduce when the error is small.
    EXPECT_GT(ciphertext.Error(), ciphertext29.Error());
  }
}

TEST_F(SymmetricRlweEncryptionTest, OperationsFailOnMismatchedPowersOfS) {
  // Cannot perform operations between ciphertexts encrypted under different
  // powers of s.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto m1, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext1, params_.get()));
  auto plaintext1_ntt =
      rlwe::Polynomial<uint_m>::ConvertToNtt(m1, ntt_params_, params_.get());
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  auto ciphertext1 =
      Ciphertext({plaintext1_ntt}, 1, error_params_->B_encryption(),
                 params_.get(), error_params_.get());
  auto ciphertext2 =
      Ciphertext({plaintext1_ntt}, 2, error_params_->B_encryption(),
                 params_.get(), error_params_.get());
  EXPECT_THAT(ciphertext1 + ciphertext2,
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be encrypted with the same key")));
  EXPECT_THAT(ciphertext1 * ciphertext2,
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be encrypted with the same key")));
}

TEST_F(SymmetricRlweEncryptionTest, AddsAndMultPreservePowerOfS) {
  // Verifies that the power of S changes as expected in adds / mults.
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(auto m1, rlwe::testing::ConvertToMontgomery<uint_m>(
                                    plaintext1, params_.get()));
  auto plaintext1_ntt =
      rlwe::Polynomial<uint_m>::ConvertToNtt(m1, ntt_params_, params_.get());
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  auto ciphertext1 =
      Ciphertext({plaintext1_ntt}, 2, error_params_->B_encryption(),
                 params_.get(), error_params_.get());
  auto ciphertext2 =
      Ciphertext({plaintext1_ntt}, 2, error_params_->B_encryption(),
                 params_.get(), error_params_.get());

  EXPECT_EQ(ciphertext1.PowerOfS(), 2);
  EXPECT_EQ(ciphertext2.PowerOfS(), 2);
  ASSERT_OK_AND_ASSIGN(auto sum, ciphertext1 + ciphertext2);
  EXPECT_EQ(sum.PowerOfS(), 2);
  ASSERT_OK_AND_ASSIGN(auto prod, ciphertext1* ciphertext2);
  EXPECT_EQ(prod.PowerOfS(), 2);
}

TEST_F(SymmetricRlweEncryptionTest, Substitutes) {
  // Tests substitutions of the form 2^k + 1.
  for (int k = 1; k < rlwe::testing::kLogCoeffs; k++) {
    int substitution_power = (1 << k) + 1;
    ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
    auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

    // Create the expected polynomial output by substituting the plaintext.
    ASSERT_OK_AND_ASSIGN(
        auto m_plaintext,
        rlwe::testing::ConvertToMontgomery<uint_m>(plaintext, params_.get()));
    auto plaintext_ntt = rlwe::Polynomial<uint_m>::ConvertToNtt(
        m_plaintext, ntt_params_, params_.get());
    ASSERT_OK_AND_ASSIGN(auto expected_ntt,
                         plaintext_ntt.Substitute(substitution_power,
                                                  ntt_params_, params_.get()));
    std::vector<uint_m::Int> expected = rlwe::RemoveError<uint_m>(
        expected_ntt.InverseNtt(ntt_params_, params_.get()), params_->modulus,
        rlwe::testing::kDefaultT, params_.get());

    // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
    ASSERT_OK_AND_ASSIGN(auto ciphertext, Encrypt(key, plaintext));
    ASSERT_OK_AND_ASSIGN(
        auto substituted,
        ciphertext.Substitute(substitution_power, ntt_params_));
    ASSERT_OK_AND_ASSIGN(auto key_sub, key.Substitute(substitution_power));
    ASSERT_OK_AND_ASSIGN(std::vector<uint_m::Int> decrypted,
                         rlwe::Decrypt<uint_m>(key_sub, substituted));

    EXPECT_EQ(decrypted, expected);
    EXPECT_EQ(substituted.PowerOfS(), substitution_power);
    EXPECT_EQ(substituted.Error(), ciphertext.Error());
  }
}

TEST_F(SymmetricRlweEncryptionTest, SubstitutionFailsOnEvenPower) {
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto enc, Encrypt(key, plaintext));
  EXPECT_THAT(enc.Substitute(2, ntt_params_),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("power must be a non-negative odd integer")));
}

TEST_F(SymmetricRlweEncryptionTest, PowerOfSUpdatedAfterRepeatedSubs) {
  int substitution_power = 5;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());
  auto plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  // Encrypt and substitute the ciphertext. Decrypt with a substituted key.
  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2,
                       ciphertext1.Substitute(substitution_power, ntt_params_));
  ASSERT_OK_AND_ASSIGN(auto ciphertext3,
                       ciphertext2.Substitute(substitution_power, ntt_params_));
  EXPECT_EQ(ciphertext3.PowerOfS(),
            (substitution_power * substitution_power) % (2 * key.Len()));
}

TEST_F(SymmetricRlweEncryptionTest, PowersOfSMustMatchOnOperations) {
  int substitution_power = 5;
  ASSERT_OK_AND_ASSIGN(auto key, SampleKey());

  std::vector<uint_m::Int> plaintext1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext2 =
      rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto ciphertext1, Encrypt(key, plaintext1));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2, Encrypt(key, plaintext2));
  ASSERT_OK_AND_ASSIGN(auto ciphertext2_sub,
                       ciphertext2.Substitute(substitution_power, ntt_params_));

  EXPECT_THAT(ciphertext1 + ciphertext2_sub,
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be encrypted with the same key")));
  EXPECT_THAT(ciphertext1 * ciphertext2_sub,
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("must be encrypted with the same key")));
}

TEST_F(SymmetricRlweEncryptionTest, NullKeyHasValueZero) {
  Polynomial zero(1 << rlwe::testing::kLogCoeffs, params_.get());

  ASSERT_OK_AND_ASSIGN(
      auto null_key,
      Key::NullKey(rlwe::testing::kLogCoeffs, rlwe::testing::kDefaultVariance,
                   rlwe::testing::kDefaultLogT, params_.get(), &ntt_params_));

  EXPECT_THAT(zero, Eq(null_key.Key()));
}

TEST_F(SymmetricRlweEncryptionTest, AddAndSubKeys) {
  ASSERT_OK_AND_ASSIGN(auto key_1, SampleKey());
  ASSERT_OK_AND_ASSIGN(auto key_2, SampleKey());

  ASSERT_OK_AND_ASSIGN(auto key_3, key_1.Add(key_2));
  ASSERT_OK_AND_ASSIGN(auto key_4, key_1.Sub(key_2));

  ASSERT_OK_AND_ASSIGN(Polynomial poly_3,
                       key_1.Key().Add(key_2.Key(), params_.get()));
  ASSERT_OK_AND_ASSIGN(Polynomial poly_4,
                       key_1.Key().Sub(key_2.Key(), params_.get()));

  EXPECT_THAT(key_3.Key(), Eq(poly_3));
  EXPECT_THAT(key_4.Key(), Eq(poly_4));
}

TEST_F(SymmetricRlweEncryptionTest, EncryptAndDecryptWithAddAndSubKeys) {
  ASSERT_OK_AND_ASSIGN(auto key_1, SampleKey());
  ASSERT_OK_AND_ASSIGN(auto key_2, SampleKey());
  ASSERT_OK_AND_ASSIGN(auto add_keys, key_1.Add(key_2));
  ASSERT_OK_AND_ASSIGN(auto sub_keys, key_1.Sub(key_2));
  std::vector<uint_m::Int> plaintext = rlwe::testing::SamplePlaintext<uint_m>();

  ASSERT_OK_AND_ASSIGN(auto add_ciphertext, Encrypt(add_keys, plaintext));
  ASSERT_OK_AND_ASSIGN(auto sub_ciphertext, Encrypt(sub_keys, plaintext));
  ASSERT_OK_AND_ASSIGN(auto decrypted_add_ciphertext,
                       rlwe::Decrypt(add_keys, add_ciphertext));
  ASSERT_OK_AND_ASSIGN(auto decrypted_sub_ciphertext,
                       rlwe::Decrypt(sub_keys, sub_ciphertext));

  EXPECT_EQ(plaintext, decrypted_add_ciphertext);
  EXPECT_EQ(plaintext, decrypted_sub_ciphertext);
}

TEST_F(SymmetricRlweEncryptionTest, IsKeyHomomorphic) {
  ASSERT_OK_AND_ASSIGN(auto prng_seed, rlwe::SingleThreadPrng::GenerateSeed());
  ASSERT_OK_AND_ASSIGN(auto prng, rlwe::SingleThreadPrng::Create(prng_seed));
  // Generate the keys.
  ASSERT_OK_AND_ASSIGN(auto key_1, SampleKey());
  ASSERT_OK_AND_ASSIGN(auto key_2, SampleKey());
  // Generate the plaintexts.
  std::vector<uint_m::Int> plaintext_1 =
      rlwe::testing::SamplePlaintext<uint_m>();
  std::vector<uint_m::Int> plaintext_2 =
      rlwe::testing::SamplePlaintext<uint_m>();
  ASSERT_OK_AND_ASSIGN(
      auto plaintext_mont_1,
      rlwe::testing::ConvertToMontgomery<uint_m>(plaintext_1, params_.get()));
  ASSERT_OK_AND_ASSIGN(
      auto plaintext_mont_2,
      rlwe::testing::ConvertToMontgomery<uint_m>(plaintext_2, params_.get()));
  auto poly_1 =
      Polynomial::ConvertToNtt(plaintext_mont_1, ntt_params_, params_.get());
  auto poly_2 =
      Polynomial::ConvertToNtt(plaintext_mont_2, ntt_params_, params_.get());
  // Compute the expected plaintexts.
  std::vector<uint_m::Int> add_plaintext = plaintext_1;
  std::vector<uint_m::Int> sub_plaintext = plaintext_2;
  std::transform(plaintext_1.begin(), plaintext_1.end(), plaintext_2.begin(),
                 add_plaintext.begin(),
                 [](uint_m::Int u, uint_m::Int v) -> uint_m::Int {
                   return (u + v) % rlwe::testing::kDefaultT;
                 });
  std::transform(
      plaintext_1.begin(), plaintext_1.end(), plaintext_2.begin(),
      sub_plaintext.begin(), [](uint_m::Int u, uint_m::Int v) -> uint_m::Int {
        return (rlwe::testing::kDefaultT + u - v) % rlwe::testing::kDefaultT;
      });

  // Sample the "a" to be used in both ciphertexts.
  ASSERT_OK_AND_ASSIGN(
      auto a, rlwe::SamplePolynomialFromPrng<uint_m>(key_1.Len(), prng.get(),
                                                     key_1.ModulusParams()));
  // Encrypt with the same a and different keys
  ASSERT_OK_AND_ASSIGN(auto poly_ciphertext_1,
                       rlwe::internal::Encrypt(key_1, poly_1, a, prng.get()));
  ASSERT_OK_AND_ASSIGN(auto poly_ciphertext_2,
                       rlwe::internal::Encrypt(key_2, poly_2, a, prng.get()));
  // Add and Substract the ciphertexts
  ASSERT_OK_AND_ASSIGN(auto add_poly_ciphertext,
                       poly_ciphertext_1.Add(poly_ciphertext_2, params_.get()));
  ASSERT_OK_AND_ASSIGN(auto sub_poly_ciphertext,
                       poly_ciphertext_1.Sub(poly_ciphertext_2, params_.get()));
  // The resulting ciphertexts should be decryptable unded the added (resp.
  // substracted) keys.
  ASSERT_OK_AND_ASSIGN(auto add_keys, key_1.Add(key_2));
  ASSERT_OK_AND_ASSIGN(auto sub_keys, key_1.Sub(key_2));
  ASSERT_OK_AND_ASSIGN(
      auto decrypted_add_ciphertext,
      rlwe::Decrypt(add_keys,
                    Ciphertext({add_poly_ciphertext, a.Negate(params_.get())},
                               1, error_params_->B_encryption(), params_.get(),
                               error_params_.get())));
  ASSERT_OK_AND_ASSIGN(
      auto decrypted_sub_ciphertext,
      rlwe::Decrypt(sub_keys,
                    Ciphertext({sub_poly_ciphertext, a.Negate(params_.get())},
                               1, error_params_->B_encryption(), params_.get(),
                               error_params_.get())));

  EXPECT_EQ(add_plaintext, decrypted_add_ciphertext);
  EXPECT_EQ(sub_plaintext, decrypted_sub_ciphertext);
}

TEST_F(SymmetricRlweEncryptionTest, CannotAddOrSubIncompatibleKeys) {
  ASSERT_OK_AND_ASSIGN(auto key_1, SampleKey(rlwe::testing::kDefaultVariance,
                                             rlwe::testing::kDefaultLogT));
  ASSERT_OK_AND_ASSIGN(auto key_2,
                       SampleKey(rlwe::testing::kDefaultVariance + 1,
                                 rlwe::testing::kDefaultLogT));
  ASSERT_OK_AND_ASSIGN(auto key_3, SampleKey(rlwe::testing::kDefaultVariance,
                                             rlwe::testing::kDefaultLogT + 1));

  EXPECT_THAT(
      key_1.Add(key_2),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("is different than the variance of this key")));
  EXPECT_THAT(
      key_1.Sub(key_2),
      StatusIs(::absl::StatusCode::kInvalidArgument,
               HasSubstr("is different than the variance of this key")));
  EXPECT_THAT(key_1.Add(key_3),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("is different than the log_t of this key")));
  EXPECT_THAT(key_1.Sub(key_3),
              StatusIs(::absl::StatusCode::kInvalidArgument,
                       HasSubstr("is different than the log_t of this key")));
}

}  // namespace
