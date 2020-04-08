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

// Defines types that are necessary for Ring-Learning with Errors (rlwe)
// encryption.

#ifndef RLWE_MONTGOMERY_H_
#define RLWE_MONTGOMERY_H_

#include <cmath>
#include <cstdint>
#include <tuple>
#include <vector>

#include "glog/logging.h"
#include "absl/base/thread_annotations.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "bits_util.h"
#include "constants.h"
#include "int256.h"
#include "prng/prng.h"
#include "serialization.pb.h"
#include "status_macros.h"
#include "statusor.h"
#include "transcription.h"

namespace rlwe {

// Forward declarations. T needs to be an unsigned integral of at least 8 bits.
template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer &&
                                           !std::is_same<T, bool>::value &&
                                           !std::numeric_limits<T>::is_signed,
                                       T>* = nullptr>
struct MontgomeryIntParams;
template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer &&
                                           !std::is_same<T, bool>::value &&
                                           !std::numeric_limits<T>::is_signed,
                                       T>* = nullptr>
class MontgomeryInt;

namespace internal {

// The input is implicitly casted to absl::uint128 before computing the bit
// length.
inline unsigned int BitLength(absl::uint128 v) {
  return 128 - CountLeadingZeros128(v);
}

// Struct to capture the "bigger int" type.
template <typename T>
struct BigInt;
// Specialization for uint8, uint16, uint32, uint64, and uint128.
template <>
struct BigInt<Uint8> {
  typedef Uint16 value_type;
};
template <>
struct BigInt<Uint16> {
  typedef Uint32 value_type;
};
template <>
struct BigInt<Uint32> {
  typedef Uint64 value_type;
};
template <>
struct BigInt<Uint64> {
  typedef absl::uint128 value_type;
};
template <>
struct BigInt<absl::uint128> {
  typedef uint256 value_type;
};

}  // namespace internal

// The parameters necessary for a Montgomery integer. Note that the template
// parameters ensure that T is an unsigned integral of at least 8 bits.
template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer &&
                                           !std::is_same<T, bool>::value &&
                                           !std::numeric_limits<T>::is_signed,
                                       T>*>
struct MontgomeryIntParams {
  // Expose Int and its greater type. BigInt is required in order to multiply
  // two Int and ensure that no overflow occurs.
  //
  // Thread safe.
  using Int = T;
  using BigInt = typename internal::BigInt<Int>::value_type;
  static const size_t bitsize_int = sizeof(Int) * 8;
  static const size_t bitsize_bigint = sizeof(BigInt) * 8;

  static rlwe::StatusOr<std::unique_ptr<MontgomeryIntParams>> Create(
      Int modulus) {
    // Check that the modulus is smaller than max(Int) / 4.
    Int most_significant_bit = modulus >> (bitsize_int - 2);
    if (most_significant_bit != 0) {
      return absl::InvalidArgumentError(absl::StrCat(
          "The modulus should be less than 2^", (bitsize_int - 2), "."));
    }
    if ((modulus % 2) == 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("The modulus should be odd."));
    }
    return absl::WrapUnique<MontgomeryIntParams>(
        new MontgomeryIntParams(modulus));
  }

  // The value R to be used in Montgomery multiplication. R will be selected as
  // 2^bitsize(Int) and hence automatically verifies R > modulus.
  const BigInt r;

  // The modulus over which these modular operations are being performed.
  const Int modulus;

  // The modulus over which these modular operations are being performed, cast
  // as a BigInt.
  const BigInt modulus_bigint;

  // The number of bits in the modulus.
  const unsigned int log_modulus;

  // The value of R taken modulo the modulus.
  const BigInt r_mod_modulus;

  // The values below, inv_modulus and inv_r, satisfy the formula:
  //     R * inv_r - modulus * inv_modulus = 1
  // Note that inv_modulus is not literally the multiplicative inverse of
  // modulus modulo R.
  const Int inv_modulus;
  const BigInt
      inv_r;  // needed to translate from Montgomery to normal representation

  // The numerator used in the Barrett reduction.
  const BigInt barrett_numerator;  // = 2^(sizeof(Int)*8) / modulus
  const BigInt
      barrett_numerator_bigint;  // = 2^(sizeof(BigInt)*8 - 1) / modulus

  inline const Int Zero() const { return 0; }
  inline const Int One() const { return 1; }
  inline const Int Two() const { return 2; }

  // Functions to perform Barrett reduction. For more details, see
  // https://en.wikipedia.org/wiki/Barrett_reduction.
  inline Int BarrettReduce(Int input) const {
    Int out =
        static_cast<Int>((this->barrett_numerator * input) >> bitsize_int);
    out = input - (out * this->modulus);
    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    return (out >= this->modulus) ? out - this->modulus : out;
  }

  inline Int BarrettReduceBigInt(BigInt input) const {
    // Will be specialized for absl::uint128, as BigBigInt = uint512 does not
    // exists.
    using BigBigInt = typename internal::BigInt<BigInt>::value_type;
    Int out = static_cast<Int>(
        (static_cast<BigBigInt>(this->barrett_numerator_bigint) * input) >>
        (bitsize_bigint - 1));
    out = static_cast<Int>(input) - (out * this->modulus);
    return (out >= this->modulus) ? out - this->modulus : out;
  }

  // Computes the serialized byte length of an integer.
  inline unsigned int SerializedSize() const { return (log_modulus + 7) / 8; }

  // Check whether (1 << log_n) fits into the underlying Int type.
  static bool DoesLogNFit(Uint64 log_n) { return (log_n < bitsize_int - 1); }

 private:
  MontgomeryIntParams(Int mod)
      : r(static_cast<BigInt>(1) << bitsize_int),
        modulus(mod),
        modulus_bigint(static_cast<BigInt>(this->modulus)),
        log_modulus(internal::BitLength(modulus)),
        r_mod_modulus(this->r % this->modulus_bigint),
        inv_modulus(static_cast<Int>(
            std::get<1>(MontgomeryIntParams::Inverses(modulus_bigint, r)))),
        inv_r(std::get<0>(MontgomeryIntParams::Inverses(modulus_bigint, r))),
        barrett_numerator(this->r / this->modulus_bigint),
        barrett_numerator_bigint(
            (static_cast<BigInt>(1) << (bitsize_bigint - 1)) /
            this->modulus_bigint) {}

  // From Hacker's Delight.
  // Computes the Montgomery inverse coefficients for r and modulus using
  // the Extended Euclidean Algorithm.
  //
  // modulus must be odd.
  // Returns a tuple of (inv_r, inv_modulus) such that:
  //     r * inv_r - modulus * inv_modulus = 1
  static std::tuple<BigInt, BigInt> Inverses(BigInt modulus_bigint,
                                             BigInt r) {  // Invariants
    //   1) sum = x * 2^w - y * modulus.
    //   2) sum is always a power of 2.
    //   3) modulus is odd.
    //   4) y is always even.
    // sum will decrease from 2^w to 2^0 = 1
    BigInt x = 1;
    BigInt y = 0;
    for (int i = bitsize_int; i > 0; i--) {
      // Ensure that x is even.
      if ((x & 1) == 1) {
        // If x is odd, make x even by adding modulus to x and changing the
        // value of y accordingly (y remains even).
        //
        //     sum = x * 2^w - y * modulus
        //     sum = (x + modulus) * 2^w - (y + 2^w) * modulus
        //
        // We can then divide the new values of x and y by 2 safely.
        x += modulus_bigint;
        y += r;
      }
      // Divide x and y by 2
      x >>= 1;
      y >>= 1;
    }
    // Return the inverses
    return std::tuple<BigInt, BigInt>(x, y);
  }
};

// Specialization of BarrettReduceBigInt for absl::uint128 because BigBigInt =
// uint512 does not exist. Instead, we do a regular modular reduction.
template <>
inline absl::uint128 MontgomeryIntParams<absl::uint128>::BarrettReduceBigInt(
    uint256 input) const {
  return static_cast<absl::uint128>(input % this->modulus_bigint);
}

// Stores an integer in Montgomery representation. The goal of this
// class is to provide a static type that differentiates between integers
// in Montgomery representation and standard integers. Once a Montgomery
// integer is created, it has the * and + operators necessary to be treated
// as another integer.
// The underlying integer type T must be unsigned and must not be bool.
// This class is thread safe.
template <typename T, std::enable_if_t<std::numeric_limits<T>::is_integer &&
                                           !std::is_same<T, bool>::value &&
                                           !std::numeric_limits<T>::is_signed,
                                       T>*>
class ABSL_MUST_USE_RESULT MontgomeryInt {
 public:
  // Expose Int and its greater type. BigInt is required in order to multiply
  // two Int and ensure that no overflow occurs. This should also be used by
  // external classes.
  using Int = T;
  using BigInt = typename internal::BigInt<Int>::value_type;

  // Expose the parameter type.
  using Params = MontgomeryIntParams<T>;

  // Static factory that converts a non-Montgomery representation integer, the
  // underlying integer type, into a Montgomery representation integer. Does not
  // take ownership of params. i.e., import "a".
  static rlwe::StatusOr<MontgomeryInt> ImportInt(Int n, Params* params) {
    BigInt result = static_cast<BigInt>(n) * params->r_mod_modulus;
    return MontgomeryInt(params->BarrettReduceBigInt(result));
  }

  static MontgomeryInt ImportZero(Params* params) {
    return MontgomeryInt(params->Zero());
  }

  static MontgomeryInt ImportOne(Params* params) {
    // 1 should be multiplied by r_mod_modulus; we load directly r_mod_modulus.
    return MontgomeryInt(static_cast<Int>(params->r_mod_modulus));
  }

  // Import a random integer using entropy from specified prng. Does not take
  // ownership of params or prng.
  template <typename Prng = rlwe::SecurePrng>
  static rlwe::StatusOr<MontgomeryInt> ImportRandom(Prng* prng,
                                                    Params* params) {
    // In order to generate unbiased randomness, we uniformly and randomly
    // sample integers in [0, 2^params->log_modulus) until the generated integer
    // is less than the modulus (i.e., we perform rejection sampling).
    RLWE_ASSIGN_OR_RETURN(Int random_int,
                          GenerateRandomInt(params->log_modulus, prng));
    while (random_int >= params->modulus) {
      RLWE_ASSIGN_OR_RETURN(random_int,
                            GenerateRandomInt(params->log_modulus, prng));
    }
    return MontgomeryInt(random_int);
  }

  static BigInt DivAndTruncate(BigInt dividend, BigInt divisor) {
    return dividend / divisor;
  }

  // No default constructor.
  MontgomeryInt() = delete;

  // Default copy constructor.
  MontgomeryInt(const MontgomeryInt& that) = default;
  MontgomeryInt& operator=(const MontgomeryInt& that) = default;

  // Convert a Montgomery representation integer back to the underlying integer.
  // i.e., export "a".
  Int ExportInt(Params* params) const {
    return params->BarrettReduceBigInt(static_cast<BigInt>(n_) * params->inv_r);
  }

  // Returns the least significant 64 bits of n.
  static Uint64 ExportUInt64(Int n) { return static_cast<Uint64>(n); }

  // Serialization.
  rlwe::StatusOr<std::string> Serialize(Params* params) const {
    // Use transcription to transform all the LogModulus() bits of input into a
    // vector of unsigned char.
    RLWE_ASSIGN_OR_RETURN(
        auto v, (TranscribeBits<Int, Uint8>({this->n_}, params->log_modulus,
                                            params->log_modulus, 8)));
    // Return a string
    return std::string(std::make_move_iterator(v.begin()),
                       std::make_move_iterator(v.end()));
  }

  static rlwe::StatusOr<std::string> SerializeVector(
      const std::vector<MontgomeryInt>& coeffs, Params* params) {
    if (coeffs.size() > kMaxNumCoeffs) {
      return absl::InvalidArgumentError(
          absl::StrCat("Number of coefficients, ", coeffs.size(),
                       ", cannot be larger than ", kMaxNumCoeffs, "."));
    } else if (coeffs.empty()) {
      return absl::InvalidArgumentError("Cannot serialize an empty vector.");
    }
    // Bits required to represent modulus.
    int bit_size = params->log_modulus;
    // Extract the values
    std::vector<Int> coeffs_values;
    coeffs_values.reserve(coeffs.size());
    for (const auto& c : coeffs) {
      coeffs_values.push_back(c.n_);
    }
    // Use transcription to transform all the bit_size bits of input into a
    // vector of unsigned char.
    RLWE_ASSIGN_OR_RETURN(
        auto v,
        (TranscribeBits<Int, Uint8>(
            coeffs_values, coeffs_values.size() * bit_size, bit_size, 8)));
    // Return a string
    return std::string(std::make_move_iterator(v.begin()),
                       std::make_move_iterator(v.end()));
  }

  static rlwe::StatusOr<MontgomeryInt> Deserialize(absl::string_view payload,
                                                   Params* params) {
    // Parse the string as unsigned char
    std::vector<Uint8> input(payload.begin(), payload.end());
    // Bits required to represent modulus.
    int bit_size = params->log_modulus;
    // Recover the coefficients from the input stream.
    RLWE_ASSIGN_OR_RETURN(
        auto coeffs_values,
        (TranscribeBits<Uint8, Int>(input, bit_size, 8, bit_size)));
    // There will be at least one coefficient in coeff_values because bit_size
    // is always expected to be positive.
    return MontgomeryInt(coeffs_values[0]);
  }

  static rlwe::StatusOr<std::vector<MontgomeryInt>> DeserializeVector(
      int num_coeffs, absl::string_view serialized, Params* params) {
    if (num_coeffs < 0) {
      return absl::InvalidArgumentError(
          "Number of coefficients must be non-negative.");
    }
    if (num_coeffs > kMaxNumCoeffs) {
      return absl::InvalidArgumentError(
          absl::StrCat("Number of coefficients, ", num_coeffs, ", cannot be ",
                       "larger than ", kMaxNumCoeffs, "."));
    }
    // Parse the string as unsigned char
    std::vector<Uint8> input(serialized.begin(), serialized.end());
    // Bits required to represent modulus.
    int bit_size = params->log_modulus;
    // Recover the coefficients from the input stream.
    RLWE_ASSIGN_OR_RETURN(auto coeffs_values,
                          (TranscribeBits<Uint8, Int>(
                              input, bit_size * num_coeffs, 8, bit_size)));
    // Check that the number of coefficients recovered is at least what is
    // expected.
    if (coeffs_values.size() < num_coeffs) {
      return absl::InvalidArgumentError("Given serialization is invalid.");
    }
    // Create a vector of Montgomery Int from the values.
    std::vector<MontgomeryInt> coeffs;
    coeffs.reserve(num_coeffs);
    for (int i = 0; i < num_coeffs; i++) {
      coeffs.push_back(MontgomeryInt(coeffs_values[i]));
    }
    return coeffs;
  }

  // Perform a multiply followed by a modular reduction. Produces an output
  // in the range [0, N).
  // Taken from Hacker's Delight chapter on Montgomery multiplication.
  MontgomeryInt Mul(const MontgomeryInt& that, Params* params) const {
    MontgomeryInt out(*this);
    return out.MulInPlace(that, params);
  }

  MontgomeryInt& MulInPlace(const MontgomeryInt& that, Params* params) {
    // This function computes the product of the two numbers (a and b), which
    // will equal a * R * b * R in Montgomery representation. It then performs
    // the reduction, creating a * b * R (mod N).
    Int u = static_cast<Int>(n_ * that.n_) * params->inv_modulus;
    BigInt t = static_cast<BigInt>(n_) * that.n_ + params->modulus_bigint * u;
    Int t_msb = static_cast<Int>(t >> Params::bitsize_int);

    // The steps above produce an integer that is in the range [0, 2N).
    // We now reduce to the range [0, N).
    n_ = (t_msb >= params->modulus) ? t_msb - params->modulus : t_msb;
    return *this;
  }

  // Montgomery addition. Performs a Barrett reduction.
  MontgomeryInt Add(const MontgomeryInt& that, Params* params) const {
    MontgomeryInt out(*this);
    return out.AddInPlace(that, params);
  }

  MontgomeryInt& AddInPlace(const MontgomeryInt& that, Params* params) {
    // We can use Barrett reduction because n_ <= modulus < Max(Int)/2.
    n_ = params->BarrettReduce(n_ + that.n_);
    return *this;
  }

  // Negation.
  MontgomeryInt Negate(Params* params) const {
    return MontgomeryInt(params->modulus - n_);
  }

  MontgomeryInt& NegateInPlace(Params* params) {
    n_ = params->modulus - n_;
    return *this;
  }

  MontgomeryInt Sub(const MontgomeryInt& that, Params* params) const {
    MontgomeryInt out(*this);
    return out.SubInPlace(that, params);
  }

  MontgomeryInt& SubInPlace(const MontgomeryInt& that, Params* params) {
    // We can use Barrett reduction because n_ <= modulus < Max(Int)/2.
    n_ = params->BarrettReduce(n_ + (params->modulus - that.n_));
    return *this;
  }

  // Batch operations (and in-place variants) over vectors of MontgomeryInt.
  // We define two versions of the batch operations:
  // -  the first input is a vector and the second is a MontgomeryInt scalar:
  //    in that case, the scalar will be added/subtracted/multiplied with each
  //    element of the first input.
  // -  both inputs are vectors of same length: in that case, the operations
  //    will be performed component wise.
  // These batch operations may fail if the input vectors are not of the same
  // size.

  // Batch addition of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchAdd(
      const std::vector<MontgomeryInt>& in1,
      const std::vector<MontgomeryInt>& in2, Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchAddInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchAddInPlace(std::vector<MontgomeryInt>* in1,
                                      const std::vector<MontgomeryInt>& in2,
                                      Params* params) {
    // If the input vectors' sizes don't match, return an error.
    if (in1->size() != in2.size()) {
      return absl::InvalidArgumentError("Input vectors are not of same size");
    }
    int i = 0;
    // The remaining elements, if any, are added in place sequentially.
    for (; i < in1->size(); i++) {
      (*in1)[i].AddInPlace(in2[i], params);
    }
    return absl::OkStatus();
  }

  // Batch addition of one vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchAdd(
      const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
      Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchAddInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchAddInPlace(std::vector<MontgomeryInt>* in1,
                                      const MontgomeryInt& in2,
                                      Params* params) {
    int i = 0;
    // The remaining elements, if any, are added in place sequentially.
    std::for_each(
        in1->begin() + i, in1->end(),
        [in2, params](MontgomeryInt& coeff) { coeff.AddInPlace(in2, params); });
    return absl::OkStatus();
  }

  // Batch subtraction of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchSub(
      const std::vector<MontgomeryInt>& in1,
      const std::vector<MontgomeryInt>& in2, Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchSubInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchSubInPlace(std::vector<MontgomeryInt>* in1,
                                      const std::vector<MontgomeryInt>& in2,
                                      Params* params) {
    // If the input vectors' sizes don't match, return an error.
    if (in1->size() != in2.size()) {
      return absl::InvalidArgumentError("Input vectors are not of same size");
    }
    int i = 0;
    // The remaining elements, if any, are subtracted in place sequentially.
    for (; i < in1->size(); i++) {
      (*in1)[i].SubInPlace(in2[i], params);
    }
    return absl::OkStatus();
  }

  // Batch subtraction of one vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchSub(
      const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
      Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchSubInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchSubInPlace(std::vector<MontgomeryInt>* in1,
                                      const MontgomeryInt& in2,
                                      Params* params) {
    int i = 0;
    // The remaining elements, if any, are subtracted in place sequentially.
    std::for_each(
        in1->begin() + i, in1->end(),
        [in2, params](MontgomeryInt& coeff) { coeff.SubInPlace(in2, params); });
    return absl::OkStatus();
  }

  // Batch multiplication of two vectors.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchMul(
      const std::vector<MontgomeryInt>& in1,
      const std::vector<MontgomeryInt>& in2, Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchMulInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchMulInPlace(std::vector<MontgomeryInt>* in1,
                                      const std::vector<MontgomeryInt>& in2,
                                      Params* params) {
    // If the input vectors' sizes don't match, return an error.
    if (in1->size() != in2.size()) {
      return absl::InvalidArgumentError("Input vectors are not of same size");
    }
    int i = 0;
    // The remaining elements, if any, are multiplied in place sequentially.
    for (; i < in1->size(); i++) {
      (*in1)[i].MulInPlace(in2[i], params);
    }
    return absl::OkStatus();
  }

  // Batch multiplication of a vector with a scalar.
  static rlwe::StatusOr<std::vector<MontgomeryInt>> BatchMul(
      const std::vector<MontgomeryInt>& in1, const MontgomeryInt& in2,
      Params* params) {
    std::vector<MontgomeryInt> out = in1;
    RLWE_RETURN_IF_ERROR(BatchMulInPlace(&out, in2, params));
    return out;
  }

  static absl::Status BatchMulInPlace(std::vector<MontgomeryInt>* in1,
                                      const MontgomeryInt& in2,
                                      Params* params) {
    int i = 0;
    // The remaining elements, if any, are multiplied in place sequentially.
    std::for_each(
        in1->begin() + i, in1->end(),
        [in2, params](MontgomeryInt& coeff) { coeff.MulInPlace(in2, params); });
    return absl::OkStatus();
  }

  // Equality.
  bool operator==(const MontgomeryInt& that) const { return (n_ == that.n_); }
  bool operator!=(const MontgomeryInt& that) const { return !(*this == that); }

  // Modular exponentiation.
  MontgomeryInt ModExp(Int exponent, Params* params) const {
    MontgomeryInt result = MontgomeryInt::ImportOne(params);
    MontgomeryInt base = *this;

    // Uses the bits of the exponent to gradually compute the result.
    // When bit k of the exponent is 1, the result is multiplied by
    // base^{2^k}.
    while (exponent > 0) {
      // If the current bit (bit k) is 1, multiply base^{2^k} into the result.
      if (exponent % 2 == 1) {
        result.MulInPlace(base, params);
      }

      // Update base from base^{2^k} to base^{2^{k+1}}.
      base.MulInPlace(base, params);
      exponent >>= 1;
    }

    return result;
  }

  // Inverse.
  MontgomeryInt MultiplicativeInverse(Params* params) const {
    return (*this).ModExp(static_cast<Int>(params->modulus - 2), params);
  }

 private:
  template <typename Prng = rlwe::SecurePrng>
  static rlwe::StatusOr<Int> GenerateRandomInt(int log_modulus, Prng* prng) {
    // Generate a random Int. As the modulus is always smaller than max(Int),
    // there will be no issues with overflow.
    int max_bits_per_step = std::min((int)Params::bitsize_int, (int)64);
    auto bits_required = log_modulus;
    Int rand = 0;
    while (bits_required > 0) {
      Int rand_bits = 0;
      if (bits_required <= 8) {
        // Generate 8 bits of randomness.
        RLWE_ASSIGN_OR_RETURN(rand_bits, prng->Rand8());

        // Extract bits_required bits and add them to rand.
        Int needed_bits =
            rand_bits & ((static_cast<Int>(1) << bits_required) - 1);
        rand = (rand << bits_required) + needed_bits;
        break;
      } else {
        // Generate 64 bits of randomness.
        RLWE_ASSIGN_OR_RETURN(rand_bits, prng->Rand64());

        // Extract min(64, bits in Int, bits_required) bits and add them to rand
        int bits_to_extract = std::min(bits_required, max_bits_per_step);
        Int needed_bits =
            rand_bits & ((static_cast<Int>(1) << bits_to_extract) - 1);
        rand = (rand << bits_to_extract) + needed_bits;
        bits_required -= bits_to_extract;
      }
    }
    return rand;
  }

  explicit MontgomeryInt(Int n) : n_(n) {}

  Int n_;
};

}  // namespace rlwe

#endif  // RLWE_MONTGOMERY_H_
