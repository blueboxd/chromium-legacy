#ifndef RLWE_TESTING_STATUS_TESTING_H_
#define RLWE_TESTING_STATUS_TESTING_H_

#include "status_macros.h"

#undef ASSERT_OK
#define ASSERT_OK(expr) \
  RLWE_ASSERT_OK_IMPL_(RLWE_STATUS_MACROS_IMPL_CONCAT_(_status, __LINE__), expr)

#define RLWE_ASSERT_OK_IMPL_(status, expr) \
  auto status = (expr);                    \
  ASSERT_THAT(status.ok(), ::testing::Eq(true));

#undef EXPECT_OK
#define EXPECT_OK(expr) \
  RLWE_EXPECT_OK_IMPL_(RLWE_STATUS_MACROS_IMPL_CONCAT_(_status, __LINE__), expr)

#define RLWE_EXPECT_OK_IMPL_(status, expr) \
  auto status = (expr);                    \
  EXPECT_THAT(status.ok(), ::testing::Eq(true));

#undef ASSERT_OK_AND_ASSIGN
#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  RLWE_ASSERT_OK_AND_ASSIGN_IMPL_(       \
      RLWE_STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, rexpr)

#define RLWE_ASSERT_OK_AND_ASSIGN_IMPL_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                    \
  ASSERT_THAT(statusor.ok(), ::testing::Eq(true));            \
  lhs = std::move(statusor).ValueOrDie()

#endif  // RLWE_TESTING_STATUS_TESTING_H_
