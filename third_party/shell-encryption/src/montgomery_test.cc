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
#include "montgomery.h"

#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <random>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"
#include "constants.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "testing/status_matchers.h"
#include "testing/status_testing.h"
#include "testing/testing_prng.h"

namespace rlwe {
namespace {

// Random uniform distribution for Uint64.
std::uniform_int_distribution<Uint64> uniform_uint64;

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

const Uint64 kTestingRounds = 1;

// Montgomery parameters for a 10-bit modulus.
constexpr Uint64 kModulus10 = 997;

// Montgomery parameters for a 8-bit modulus.
constexpr Uint64 kModulus8 = 233;

// Generate a random integer of a specified number of bits.
template <class TypeParam>
TypeParam GenerateRandom(unsigned int* seed) {
  std::minstd_rand random(*seed);
  *seed += 1;
  return static_cast<TypeParam>(uniform_uint64(random));
}
// Specialization for absl::uint128 and uint256.
template <>
absl::uint128 GenerateRandom(unsigned int* seed) {
  Uint64 hi = GenerateRandom<Uint64>(seed);
  Uint64 lo = GenerateRandom<Uint64>(seed);
  return absl::MakeUint128(hi, lo);
}
template <>
uint256 GenerateRandom(unsigned int* seed) {
  absl::uint128 hi = GenerateRandom<absl::uint128>(seed);
  absl::uint128 lo = GenerateRandom<absl::uint128>(seed);
  return uint256(hi, lo);
}

template <typename T>
std::list<std::unique_ptr<MontgomeryIntParams<T>>> ConstructParams();

template <>
std::list<std::unique_ptr<MontgomeryIntParams<Uint16>>>
ConstructParams<Uint16>() {
  std::list<std::unique_ptr<MontgomeryIntParams<Uint16>>> params;
  for (auto modulus :
       std::vector<Uint16>({kModulus8, kModulus10, kNewhopeModulus})) {
    auto status_or_params = MontgomeryIntParams<Uint16>::Create(modulus);
    if (!status_or_params.ok()) {
      LOG(FATAL) << "Creating MontgomeryIntParams failed.\n";
    }
    params.emplace_back(std::move(status_or_params).ValueOrDie());
  }
  return params;
}

template <>
std::list<std::unique_ptr<MontgomeryIntParams<Uint32>>>
ConstructParams<Uint32>() {
  std::list<std::unique_ptr<MontgomeryIntParams<Uint32>>> params;
  for (auto modulus : std::vector<Uint32>(
           {kModulus8, kModulus10, kNewhopeModulus, kModulus29})) {
    auto status_or_params = MontgomeryIntParams<Uint32>::Create(modulus);
    if (!status_or_params.ok()) {
      LOG(FATAL) << "Creating MontgomeryIntParams failed.\n";
    }
    params.emplace_back(std::move(status_or_params).ValueOrDie());
  }
  return params;
}

template <>
std::list<std::unique_ptr<MontgomeryIntParams<Uint64>>>
ConstructParams<Uint64>() {
  std::list<std::unique_ptr<MontgomeryIntParams<Uint64>>> params;
  for (auto modulus : std::vector<Uint64>(
           {kModulus8, kModulus10, kNewhopeModulus, kModulus29, kModulus59})) {
    auto status_or_params = MontgomeryIntParams<Uint64>::Create(modulus);
    if (!status_or_params.ok()) {
      LOG(FATAL) << "Creating MontgomeryIntParams failed.\n";
    }
    params.emplace_back(std::move(status_or_params).ValueOrDie());
  }
  return params;
}

template <>
std::list<std::unique_ptr<MontgomeryIntParams<absl::uint128>>>
ConstructParams<absl::uint128>() {
  std::list<std::unique_ptr<MontgomeryIntParams<absl::uint128>>> params;
  for (auto modulus : std::vector<absl::uint128>({kModulus59, kModulus80})) {
    auto status_or_params = MontgomeryIntParams<absl::uint128>::Create(modulus);
    if (!status_or_params.ok()) {
      LOG(FATAL) << "Creating MontgomeryIntParams failed.\n";
    }
    params.emplace_back(std::move(status_or_params).ValueOrDie());
  }
  return params;
}

template <typename T>
class MontgomeryTest : public ::testing::Test {
 protected:
  MontgomeryTest() { params_ = ConstructParams<T>(); }

  std::list<std::unique_ptr<MontgomeryIntParams<T>>> params_;
};
using IntTypes = ::testing::Types<Uint16, Uint32, Uint64, absl::uint128>;
TYPED_TEST_SUITE(MontgomeryTest, IntTypes);

TYPED_TEST(MontgomeryTest, ModulusTooLarge) {
  using Int = TypeParam;

  unsigned int seed = 0;
  Int modulus;
  for (Int i = 0; i < kTestingRounds; ++i) {
    // Sample an invalid odd modulus in (max(Int)/2, max(Int)).
    modulus =
        (std::numeric_limits<Int>::max() / 2) +
        (GenerateRandom<Int>(&seed) % (std::numeric_limits<Int>::max() / 2));
    modulus |= 1;  // Ensure that the modulus is odd.

    EXPECT_THAT(
        MontgomeryIntParams<Int>::Create(modulus),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat("The modulus should be less than 2^",
                                        (sizeof(Int) * 8 - 2), "."))));

    // Sample an even modulus in the allowed range.
    modulus =
        (GenerateRandom<Int>(&seed) % (std::numeric_limits<Int>::max() / 8))
        << 1;
    EXPECT_THAT(
        MontgomeryIntParams<Int>::Create(modulus),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr(absl::StrCat("The modulus should be odd."))));
  }
}

// Verifies that the MontgomeryIntParams code computes the inverse modulus.
TYPED_TEST(MontgomeryTest, ParamsInvModulus) {
  using BigInt = typename internal::BigInt<TypeParam>::value_type;
  for (auto& params : this->params_) {
    EXPECT_EQ(params->r * params->inv_r -
                  static_cast<BigInt>(params->modulus) * params->inv_modulus,
              1);
  }
}

// Verifies that numbers can be imported and exported properly.
TYPED_TEST(MontgomeryTest, ImportExportInt) {
  using Int = TypeParam;

  unsigned int seed = 0;
  for (auto& params : this->params_) {
    for (Int i = 0; i < kTestingRounds; ++i) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(auto m,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      Int after = m.ExportInt(params.get());
      EXPECT_EQ(after, a);
    }
  }
}

// Verifies that numbers can be added correctly.
TYPED_TEST(MontgomeryTest, AddSub) {
  using Int = TypeParam;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      Int b = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(MontgomeryInt<Int> ma,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(MontgomeryInt<Int> mb,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      MontgomeryInt<Int> mc = ma.Add(mb, params.get());
      Int c = mc.ExportInt(params.get());

      Int expected = (a + b) % params->modulus;
      EXPECT_EQ(expected, c);

      MontgomeryInt<Int> md = ma.Sub(mb, params.get());
      Int d = md.ExportInt(params.get());

      Int expected2 = (a + params->modulus - b) % params->modulus;
      EXPECT_EQ(expected2, d);
    }
  }
}

TYPED_TEST(MontgomeryTest, InlineAddSub) {
  using Int = TypeParam;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      Int b = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(MontgomeryInt<Int> ma,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(MontgomeryInt<Int> mb,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      ma.AddInPlace(mb, params.get());
      Int c = ma.ExportInt(params.get());

      Int expected = (a + b) % params->modulus;
      EXPECT_EQ(expected, c);

      ASSERT_OK_AND_ASSIGN(ma, MontgomeryInt<Int>::ImportInt(a, params.get()));

      ma.SubInPlace(mb, params.get());
      Int d = ma.ExportInt(params.get());

      Int expected2 = (a + params->modulus - b) % params->modulus;
      EXPECT_EQ(expected2, d);
    }
  }
}

// Verifies that equality functions properly.
TYPED_TEST(MontgomeryTest, Equality) {
  using Int = TypeParam;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      Int b = GenerateRandom<Int>(&seed) % params->modulus;
      while (b == a) {
        b = GenerateRandom<Int>(&seed) % params->modulus;
      }

      ASSERT_OK_AND_ASSIGN(auto ma1,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(auto ma2,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb1,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb2,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));

      EXPECT_TRUE(ma1 == ma2);
      EXPECT_TRUE(ma2 == ma1);
      EXPECT_FALSE(ma1 != ma2);
      EXPECT_FALSE(ma2 != ma1);

      EXPECT_TRUE(mb1 == mb2);
      EXPECT_TRUE(mb2 == mb1);
      EXPECT_FALSE(mb1 != mb2);
      EXPECT_FALSE(mb2 != mb1);

      EXPECT_TRUE(ma1 != mb1);
      EXPECT_TRUE(mb1 != ma1);
      EXPECT_FALSE(ma1 == mb1);
      EXPECT_FALSE(mb1 == ma1);
    }
  }
}

// Verifies that numbers can be negated correctly.
TYPED_TEST(MontgomeryTest, Negate) {
  using Int = TypeParam;

  for (auto& params : this->params_) {
    for (Int i = 0; i < 4 * kNewhopeModulus; i++) {
      ASSERT_OK_AND_ASSIGN(auto mi,
                           MontgomeryInt<Int>::ImportInt(i, params.get()));
      EXPECT_EQ(0, mi.Add(mi.Negate(params.get()), params.get())
                       .ExportInt(params.get()));
    }
  }
}

// Verifies that repeated addition works properly.
TYPED_TEST(MontgomeryTest, AddRepeatedly) {
  using Int = TypeParam;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      Int sum = 0;
      Int diff = 0;
      MontgomeryInt<Int> mont_sum =
          MontgomeryInt<Int>::ImportZero(params.get());
      MontgomeryInt<Int> mont_diff =
          MontgomeryInt<Int>::ImportZero(params.get());

      for (int j = 0; j < 1000; j++) {
        Int a = GenerateRandom<Int>(&seed) % params->modulus;

        sum = (sum + a) % params->modulus;
        ASSERT_OK_AND_ASSIGN(auto ma,
                             MontgomeryInt<Int>::ImportInt(a, params.get()));
        mont_sum = mont_sum.Add(ma, params.get());

        diff = (diff + params->modulus - a) % params->modulus;
        mont_diff = mont_diff.Sub(ma, params.get());
      }

      EXPECT_EQ(sum, mont_sum.ExportInt(params.get()));
      EXPECT_EQ(diff, mont_diff.ExportInt(params.get()));
    }
  }
}

// Verifies that numbers can be multiplied correctly.
TYPED_TEST(MontgomeryTest, Multiply) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    // Test over many random values.
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      Int b = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(auto ma,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      MontgomeryInt<Int> mc = ma.Mul(mb, params.get());
      Int c = mc.ExportInt(params.get());

      Int expected =
          static_cast<Int>((static_cast<BigInt>(a) * static_cast<BigInt>(b)) %
                           static_cast<BigInt>(params->modulus));
      EXPECT_EQ(expected, c);
    }
    // Test the multiplication of the maximum values together.
    Int a = params->modulus - 1;
    ASSERT_OK_AND_ASSIGN(auto ma,
                         MontgomeryInt<Int>::ImportInt(a, params.get()));

    MontgomeryInt<Int> mb = ma.Mul(ma, params.get());
    Int b = mb.ExportInt(params.get());

    EXPECT_EQ(1, b);
  }
}

TYPED_TEST(MontgomeryTest, InlineMultiply) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    // Test over many random values.
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      Int b = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(auto ma,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      ma.MulInPlace(mb, params.get());
      Int c = ma.ExportInt(params.get());

      Int expected =
          static_cast<Int>((static_cast<BigInt>(a) * static_cast<BigInt>(b)) %
                           static_cast<BigInt>(params->modulus));
      EXPECT_EQ(expected, c);
    }
  }
}

// Verifies that repeated addition works properly.
TYPED_TEST(MontgomeryTest, MultiplyRepeatedly) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      BigInt prod = 1;
      MontgomeryInt<Int> mont_prod =
          MontgomeryInt<Int>::ImportOne(params.get());

      for (int j = 0; j < 1000; j++) {
        Int a = GenerateRandom<Int>(&seed) % params->modulus;

        prod = (prod * static_cast<BigInt>(a)) %
               static_cast<BigInt>(params->modulus);
        ASSERT_OK_AND_ASSIGN(auto ma,
                             MontgomeryInt<Int>::ImportInt(a, params.get()));
        mont_prod = mont_prod.Mul(ma, params.get());
      }

      EXPECT_EQ(static_cast<Int>(prod), mont_prod.ExportInt(params.get()));
    }
  }
}

// Test the entire space for a small modulus.
TYPED_TEST(MontgomeryTest, SmallModulus) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;

  for (auto& params : this->params_) {
    const BigInt modulus = static_cast<BigInt>(params->modulus);
    Int upper_bound = std::min<Int>(kModulus8, params->modulus);
    for (Int a = 0; a < 2 * upper_bound; a++) {
      Int b = a + 1;
      BigInt a_BigInt = static_cast<BigInt>(a);
      BigInt b_BigInt = static_cast<BigInt>(b);
      ASSERT_OK_AND_ASSIGN(auto ma,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      ASSERT_OK_AND_ASSIGN(auto mb,
                           MontgomeryInt<Int>::ImportInt(b, params.get()));
      MontgomeryInt<Int> mc = ma.Add(mb, params.get());

      // Equality.
      if (a_BigInt % modulus == b_BigInt % modulus) {
        EXPECT_TRUE(ma == mb);
        EXPECT_FALSE(ma != mb);
      } else {
        EXPECT_TRUE(ma != mb);
        EXPECT_FALSE(ma == mb);
      }

      // Addition.
      EXPECT_EQ(static_cast<Int>((a_BigInt + b_BigInt) % modulus),
                mc.ExportInt(params.get()));

      // Negation.
      EXPECT_EQ(static_cast<Int>((2 * modulus - a_BigInt) % modulus),
                (ma.Negate(params.get())).ExportInt(params.get()));
      EXPECT_EQ(static_cast<Int>((2 * modulus - b_BigInt) % modulus),
                (mb.Negate(params.get())).ExportInt(params.get()));
      EXPECT_EQ(static_cast<Int>((4 * modulus - a_BigInt - b_BigInt) % modulus),
                (mc.Negate(params.get())).ExportInt(params.get()));

      // Subtraction.
      EXPECT_EQ(static_cast<Int>((2 * modulus - a_BigInt + b_BigInt) % modulus),
                (mb.Sub(ma, params.get()).ExportInt(params.get())));
      EXPECT_EQ(static_cast<Int>((2 * modulus - b_BigInt + a_BigInt) % modulus),
                (ma.Sub(mb, params.get()).ExportInt(params.get())));

      // Multiplication and commutativity.
      EXPECT_EQ(static_cast<Int>((a_BigInt * b_BigInt) % modulus),
                (ma.Mul(mb, params.get())).ExportInt(params.get()));
      EXPECT_EQ(static_cast<Int>((a_BigInt * b_BigInt) % modulus),
                (mb.Mul(ma, params.get())).ExportInt(params.get()));
    }
  }
}

TYPED_TEST(MontgomeryTest, ModExpModulus) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    const BigInt modulus = static_cast<BigInt>(params->modulus);
    for (int i = 0; i < kTestingRounds; i++) {
      BigInt expected = 1;
      Int base = GenerateRandom<Int>(&seed) % params->modulus;
      for (Int exp = 0; exp < 1000; exp++) {
        ASSERT_OK_AND_ASSIGN(auto base_m,
                             MontgomeryInt<Int>::ImportInt(base, params.get()));
        auto actual_m = base_m.ModExp(exp, params.get());
        Int actual = actual_m.ExportInt(params.get());
        ASSERT_EQ(actual, expected);

        expected *= static_cast<BigInt>(base);
        expected %= modulus;
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, InverseModulus) {
  using Int = TypeParam;
  unsigned int seed = 0;

  for (auto& params : this->params_) {
    for (int i = 0; i < kTestingRounds; i++) {
      Int a = GenerateRandom<Int>(&seed) % params->modulus;
      ASSERT_OK_AND_ASSIGN(auto a_m,
                           MontgomeryInt<Int>::ImportInt(a, params.get()));
      MontgomeryInt<Int> inv = a_m.MultiplicativeInverse(params.get());
      ASSERT_EQ((a_m.Mul(inv, params.get())).ExportInt(params.get()), 1);
    }
  }
}

TYPED_TEST(MontgomeryTest, Serialization) {
  using Int = TypeParam;

  // Try all possible values up to kModulus10.
  for (auto& params : this->params_) {
    for (Int i = 0; i < kModulus10; i++) {
      Int input_int = i % params->modulus;
      // Serialize and ensure the byte length is as expected.
      ASSERT_OK_AND_ASSIGN(auto int_value, MontgomeryInt<Int>::ImportInt(
                                               input_int, params.get()));

      ASSERT_OK_AND_ASSIGN(std::string serialized,
                           int_value.Serialize(params.get()));

      EXPECT_EQ(serialized.length(), params->SerializedSize());

      // Ensure that deserialization works properly.
      ASSERT_OK_AND_ASSIGN(
          auto int_deserialized,
          MontgomeryInt<Int>::Deserialize(serialized, params.get()));
      EXPECT_EQ(int_deserialized, int_value);

      // Ensure that that any bit beyond bit the serialized bit length can be
      // wiped out without issue. That is, ensure that the bit size is accurate.
      serialized[serialized.length() - 1] &=
          (static_cast<Uint8>(1)
           << (params->log_modulus - 8 * (serialized.length() - 1))) -
          1;
      ASSERT_OK_AND_ASSIGN(
          auto int_deserialized2,
          MontgomeryInt<Int>::Deserialize(serialized, params.get()));
      EXPECT_EQ(int_deserialized2, int_value);
    }
  }
}

TYPED_TEST(MontgomeryTest, ExceedMaxNumCoeffVectorSerialization) {
  using Int = TypeParam;
  int num_coeffs = kMaxNumCoeffs + 1;

  for (auto& params : this->params_) {
    std::vector<MontgomeryInt<Int>> coeffs;
    for (int i = 0; i < num_coeffs; ++i) {
      coeffs.push_back(MontgomeryInt<Int>::ImportOne(params.get()));
    }
    EXPECT_THAT(MontgomeryInt<Int>::SerializeVector(coeffs, params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "Number of coefficients, ", num_coeffs,
                             ", cannot be larger than ", kMaxNumCoeffs, "."))));
  }
}

TYPED_TEST(MontgomeryTest, EmptyVectorSerialization) {
  using Int = TypeParam;

  for (auto& params : this->params_) {
    std::vector<MontgomeryInt<Int>> coeffs;
    EXPECT_THAT(MontgomeryInt<Int>::SerializeVector(coeffs, params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("Cannot serialize an empty vector")));
  }
}

TYPED_TEST(MontgomeryTest, VectorSerialization) {
  using Int = TypeParam;

  // Prng to generate random values
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(0);

  for (auto& params : this->params_) {
    for (int num_coeffs = 3; num_coeffs <= 25; ++num_coeffs) {
      std::vector<MontgomeryInt<Int>> coeffs;
      coeffs.reserve(num_coeffs);
      for (int i = 0; i < num_coeffs; ++i) {
        ASSERT_OK_AND_ASSIGN(auto int_value, MontgomeryInt<Int>::ImportRandom(
                                                 prng.get(), params.get()));
        coeffs.push_back(int_value);
      }
      ASSERT_OK_AND_ASSIGN(
          std::string serialized,
          MontgomeryInt<Int>::SerializeVector(coeffs, params.get()));
      int expected_size = (num_coeffs * params->log_modulus + 7) / 8;
      EXPECT_EQ(serialized.size(), expected_size);
      ASSERT_OK_AND_ASSIGN(auto deserialized,
                           MontgomeryInt<Int>::DeserializeVector(
                               num_coeffs, serialized, params.get()));
      EXPECT_EQ(deserialized.size(), num_coeffs);
      for (int i = 0; i < num_coeffs; ++i) {
        EXPECT_EQ(coeffs[i], deserialized[i]);
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, ExceedMaxNumCoeffVectorDeserialization) {
  using Int = TypeParam;
  int num_coeffs = kMaxNumCoeffs + 1;
  for (auto& params : this->params_) {
    EXPECT_THAT(MontgomeryInt<Int>::DeserializeVector(num_coeffs, std::string(),
                                                      params.get()),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(absl::StrCat(
                             "Number of coefficients, ", num_coeffs,
                             ", cannot be larger than ", kMaxNumCoeffs, "."))));
  }
}

TYPED_TEST(MontgomeryTest, NegativeVectorDeserialization) {
  using Int = TypeParam;
  int num_coeffs = -1;
  for (auto& params : this->params_) {
    EXPECT_THAT(
        MontgomeryInt<Int>::DeserializeVector(num_coeffs, std::string(),
                                              params.get()),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr("Number of coefficients must be non-negative.")));
  }
}

TYPED_TEST(MontgomeryTest, ImportRandomWithPrngWithSameKeys) {
  using Int = TypeParam;

  unsigned int seed = 0;

  for (auto& params : this->params_) {
    unsigned int seed_prng = GenerateRandom<unsigned int>(&seed);
    auto prng1 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);
    auto prng2 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);

    ASSERT_OK_AND_ASSIGN(
        auto r1, MontgomeryInt<Int>::ImportRandom(prng1.get(), params.get()));
    ASSERT_OK_AND_ASSIGN(
        auto r2, MontgomeryInt<Int>::ImportRandom(prng2.get(), params.get()));
    EXPECT_EQ(r1, r2);
  }
}

TYPED_TEST(MontgomeryTest, ImportRandomWithPrngWithDifferentKeys) {
  using Int = TypeParam;

  unsigned int seed = 0;

  for (auto& params : this->params_) {
    unsigned int seed_prng1 = GenerateRandom<unsigned int>(&seed);
    unsigned int seed_prng2 = seed_prng1 + 1;  // Different seed
    auto prng1 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng1);
    auto prng2 = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng2);

    ASSERT_OK_AND_ASSIGN(
        auto r1, MontgomeryInt<Int>::ImportRandom(prng1.get(), params.get()));
    ASSERT_OK_AND_ASSIGN(
        auto r2, MontgomeryInt<Int>::ImportRandom(prng2.get(), params.get()));
    EXPECT_NE(r1, r2);
  }
}

// Verifies that Barrett reductions functions properly.
TYPED_TEST(MontgomeryTest, VerifyBarrett) {
  using Int = TypeParam;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Test over a selection of the possible input space.
  for (unsigned int j = 0; j < kTestingRounds; j++) {
    unsigned int seed = j;

    for (auto& params : this->params_) {
      const BigInt modulus = static_cast<BigInt>(params->modulus);
      for (int i = 0; i < kTestingRounds; i++) {
        // Verify Barrett reduction up to max(Int).
        Int a = params->modulus +
                (GenerateRandom<Int>(&seed) %
                 (std::numeric_limits<Int>::max() - params->modulus));
        EXPECT_EQ(params->BarrettReduce(a), static_cast<BigInt>(a) % modulus);

        // Verify Barrett reduction for BigInt up to max(Int) * modulus.
        BigInt b = GenerateRandom<BigInt>(&seed) %
                   (std::numeric_limits<Int>::max() * params->modulus);
        EXPECT_EQ(params->BarrettReduceBigInt(b), b % modulus);
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, BatchOperations) {
  using Int = TypeParam;
  unsigned int seed = 0;
  unsigned int seed_prng = GenerateRandom<unsigned int>(&seed);
  auto prng = absl::make_unique<rlwe::testing::TestingPrng>(seed_prng);

  for (auto& params : this->params_) {
    for (size_t length : {1, 2, 7, 32, 500, 1024}) {
      std::vector<MontgomeryInt<Int>> a, b;
      std::vector<MontgomeryInt<Int>> expected_add, expected_sub, expected_mul;
      MontgomeryInt<Int> scalar =
          MontgomeryInt<Int>::ImportRandom(prng.get(), params.get())
              .ValueOrDie();
      std::vector<MontgomeryInt<Int>> expected_add_scalar, expected_sub_scalar,
          expected_mul_scalar;
      for (size_t i = 0; i < length; i++) {
        a.push_back(MontgomeryInt<Int>::ImportRandom(prng.get(), params.get())
                        .ValueOrDie());
        b.push_back(MontgomeryInt<Int>::ImportRandom(prng.get(), params.get())
                        .ValueOrDie());
        expected_add.push_back(a[i].Add(b[i], params.get()));
        expected_sub.push_back(a[i].Sub(b[i], params.get()));
        expected_mul.push_back(a[i].Mul(b[i], params.get()));
        expected_add_scalar.push_back(a[i].Add(scalar, params.get()));
        expected_sub_scalar.push_back(a[i].Sub(scalar, params.get()));
        expected_mul_scalar.push_back(a[i].Mul(scalar, params.get()));
      }

      ASSERT_OK_AND_ASSIGN(std::vector<MontgomeryInt<Int>> add,
                           MontgomeryInt<Int>::BatchAdd(a, b, params.get()));
      ASSERT_OK_AND_ASSIGN(std::vector<MontgomeryInt<Int>> sub,
                           MontgomeryInt<Int>::BatchSub(a, b, params.get()));
      ASSERT_OK_AND_ASSIGN(std::vector<MontgomeryInt<Int>> mul,
                           MontgomeryInt<Int>::BatchMul(a, b, params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<MontgomeryInt<Int>> add_scalar,
          MontgomeryInt<Int>::BatchAdd(a, scalar, params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<MontgomeryInt<Int>> sub_scalar,
          MontgomeryInt<Int>::BatchSub(a, scalar, params.get()));
      ASSERT_OK_AND_ASSIGN(
          std::vector<MontgomeryInt<Int>> mul_scalar,
          MontgomeryInt<Int>::BatchMul(a, scalar, params.get()));

      EXPECT_EQ(add.size(), expected_add.size());
      EXPECT_EQ(sub.size(), expected_sub.size());
      EXPECT_EQ(mul.size(), expected_mul.size());
      EXPECT_EQ(add_scalar.size(), expected_add_scalar.size());
      EXPECT_EQ(sub_scalar.size(), expected_sub_scalar.size());
      EXPECT_EQ(mul_scalar.size(), expected_mul_scalar.size());
      for (size_t i = 0; i < length; i++) {
        EXPECT_EQ(add[i].ExportInt(params.get()),
                  expected_add[i].ExportInt(params.get()));
        EXPECT_EQ(sub[i].ExportInt(params.get()),
                  expected_sub[i].ExportInt(params.get()));
        EXPECT_EQ(mul[i].ExportInt(params.get()),
                  expected_mul[i].ExportInt(params.get()));
        EXPECT_EQ(add_scalar[i].ExportInt(params.get()),
                  expected_add_scalar[i].ExportInt(params.get()));
        EXPECT_EQ(sub_scalar[i].ExportInt(params.get()),
                  expected_sub_scalar[i].ExportInt(params.get()));
        EXPECT_EQ(mul_scalar[i].ExportInt(params.get()),
                  expected_mul_scalar[i].ExportInt(params.get()));
      }
    }
  }
}

TYPED_TEST(MontgomeryTest, BatchOperationsFailsWithVectorsOfDifferentSize) {
  using Int = TypeParam;
  for (auto& params : this->params_) {
    for (size_t length_a : {1, 2, 7, 32, 500, 1024}) {
      for (size_t length_b : {1, 2, 7, 32, 500, 1024}) {
        if (length_a != length_b) {
          std::vector<MontgomeryInt<Int>> a(
              length_a, MontgomeryInt<Int>::ImportZero(params.get()));
          std::vector<MontgomeryInt<Int>> b(
              length_b, MontgomeryInt<Int>::ImportZero(params.get()));

          EXPECT_THAT(
              MontgomeryInt<Int>::BatchAdd(a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              MontgomeryInt<Int>::BatchAddInPlace(&a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              MontgomeryInt<Int>::BatchSub(a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              MontgomeryInt<Int>::BatchSubInPlace(&a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              MontgomeryInt<Int>::BatchMul(a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
          EXPECT_THAT(
              MontgomeryInt<Int>::BatchMulInPlace(&a, b, params.get()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input vectors are not of same size")));
        }
      }
    }
  }
}

// This PRNG tests templating with a Prng that does not inherit from SecurePrng.
class FakePrng {
 public:
  StatusOr<Uint8> Rand8() { return 0; }
  StatusOr<Uint64> Rand64() { return 0; }
};

TYPED_TEST(MontgomeryTest, PrngTemplateParameterizationWorks) {
  using Int = TypeParam;
  FakePrng prng;
  auto& params = this->params_.front();
  ASSERT_OK(MontgomeryInt<Int>::ImportRandom(
      &prng, params.get()));
}

}  // namespace
}  // namespace rlwe
