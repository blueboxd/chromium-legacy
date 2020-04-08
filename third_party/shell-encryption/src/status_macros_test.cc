#include "status_macros.h"

#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "statusor.h"

namespace rlwe {
namespace {

TEST(StatusMacrosTest, TestAssignOrReturn) {
  StatusOr<StatusOr<int>> a(StatusOr<int>(2));
  auto f = [&]() -> absl::Status {
    RLWE_ASSIGN_OR_RETURN(StatusOr<int> status_or_a, a.ValueOrDie());
    EXPECT_EQ(2, status_or_a.ValueOrDie());
    return absl::OkStatus();
  };
  auto status = f();
  EXPECT_TRUE(status.ok()) << status;
}

TEST(StatusMacrosTest, TestAssignOrReturnFails) {
  auto a = []() -> StatusOr<int> { return absl::InternalError("error"); };
  auto f = [&]() -> absl::Status {
    RLWE_ASSIGN_OR_RETURN(auto result, a());
    result++;
    return absl::OkStatus();
  };
  auto status = f();
  EXPECT_EQ(absl::StatusCode::kInternal, status.code());
  EXPECT_EQ("error", status.message());
}

}  // namespace
}  // namespace rlwe
