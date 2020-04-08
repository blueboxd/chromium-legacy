#include "statusor.h"

#include <memory>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include <gtest/gtest.h>

namespace rlwe {
namespace {

class NoDefault {
 public:
  explicit NoDefault(int dummy) { dummy_ = dummy; }

  ~NoDefault() = default;

  int GetDummy() { return dummy_; }

 private:
  NoDefault() = delete;
  int dummy_;
};

class NoDefaultNoCopy {
 public:
  explicit NoDefaultNoCopy(int dummy) { dummy_ = dummy; }

  ~NoDefaultNoCopy() = default;

  int GetDummy() { return dummy_; }

  NoDefaultNoCopy(NoDefaultNoCopy&& other) = default;
  NoDefaultNoCopy& operator=(NoDefaultNoCopy&& other) = default;

 private:
  NoDefaultNoCopy() = delete;
  NoDefaultNoCopy(const NoDefaultNoCopy& other) = delete;
  NoDefaultNoCopy& operator=(const NoDefaultNoCopy& other) = delete;
  int dummy_;
};

TEST(StatusOrTest, StatusUnknown) {
  StatusOr<std::string> statusor;
  EXPECT_FALSE(statusor.ok());
  EXPECT_EQ(absl::UnknownError(""), statusor.status());
}

TEST(StatusOrTest, CopyCtors) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie().GetDummy());
  statusor = StatusOr<NoDefault>(no_default);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie().GetDummy());
}

TEST(StatusOrTest, MoveCtors) {
  StatusOr<NoDefaultNoCopy> statusor(NoDefaultNoCopy(42));
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie().GetDummy());
  statusor = NoDefaultNoCopy(42);
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie().GetDummy());
}

TEST(StatusOrTest, DiesWithNotOkStatus) {
  StatusOr<NoDefault> statusor(absl::CancelledError(""));
  EXPECT_DEATH_IF_SUPPORTED(statusor.ValueOrDie(), "");
}

TEST(StatusOrTest, Pointers) {
  std::unique_ptr<NoDefault> no_default(new NoDefault(42));
  StatusOr<NoDefault*> statusor(no_default.get());
  EXPECT_TRUE(statusor.ok());
  EXPECT_EQ(42, statusor.ValueOrDie()->GetDummy());
}

TEST(StatusOrTest, TestStatusOrCopyCtors) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  StatusOr<NoDefault> statusor_wrap(statusor);
  EXPECT_TRUE(statusor_wrap.ok());
  EXPECT_EQ(42, statusor_wrap.ValueOrDie().GetDummy());
}

TEST(StatusOrTest, TestStatusOrCopyAssignment) {
  NoDefault no_default(42);
  StatusOr<NoDefault> statusor(no_default);
  StatusOr<NoDefault> statusor_wrap = statusor;
  EXPECT_TRUE(statusor_wrap.ok());
  EXPECT_EQ(42, statusor_wrap.ValueOrDie().GetDummy());
}

}  // namespace
}  // namespace rlwe
