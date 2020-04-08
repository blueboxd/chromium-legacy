#include "testing/protobuf_matchers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "testing/coefficient_polynomial.pb.h"
#include "testing/status_testing.h"

namespace {

TEST(EqualsProtoTest, EqualsProtoWorks) {
  rlwe::SerializedCoefficientPolynomial coeffs;
  coeffs.set_num_coeffs(10);
  EXPECT_THAT(coeffs, rlwe::testing::EqualsProto(coeffs));

  rlwe::SerializedCoefficientPolynomial coeffs_other;
  coeffs_other.set_num_coeffs(20);
  EXPECT_THAT(coeffs, ::testing::Not(rlwe::testing::EqualsProto(coeffs_other)));
}

}  // namespace
