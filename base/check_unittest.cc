// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class which expects a check to fire with a certain location and
// message before the end of the current scope.
class ScopedCheckExpectation {
 public:
  ScopedCheckExpectation(const char* file, int line, std::string msg)
      : file_(file),
        line_(line),
        msg_(msg),
        assert_handler_(base::BindRepeating(&ScopedCheckExpectation::Check,
                                            base::Unretained(this))),
        fired_(false) {}
  ~ScopedCheckExpectation() {
    EXPECT_TRUE(fired_) << "CHECK at " << file_ << ":" << line_
                        << " never fired!";
  }

 private:
  void Check(const char* file,
             int line,
             const base::StringPiece msg,
             const base::StringPiece stack) {
    fired_ = true;
    EXPECT_EQ(file, file_);
    EXPECT_EQ(line, line_);
    if (msg_.find("=~") == 0) {
      EXPECT_THAT(std::string(msg), testing::MatchesRegex(msg_.substr(2)));
    } else {
      EXPECT_EQ(std::string(msg), msg_);
    }
  }

  std::string file_;
  int line_;
  std::string msg_;
  logging::ScopedLogAssertHandler assert_handler_;
  bool fired_;
};

// Macro which expects a CHECK to fire with a certain message. If msg starts
// with "=~", it's interpreted as a regular expression.
// Example: EXPECT_CHECK("Check failed: false.", CHECK(false));
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
#define EXPECT_CHECK(msg, check_expr) \
  do {                                \
    EXPECT_DEATH(check_expr, "");     \
  } while (0)
#else
#define EXPECT_CHECK(msg, check_expr)                          \
  do {                                                         \
    ScopedCheckExpectation check_exp(__FILE__, __LINE__, msg); \
    check_expr;                                                \
  } while (0)
#endif

// Macro which expects a DCHECK to fire if DCHECKs are enabled.
#define EXPECT_DCHECK(msg, check_expr)                                 \
  do {                                                                 \
    if (DCHECK_IS_ON() && logging::LOG_DCHECK == logging::LOG_FATAL) { \
      ScopedCheckExpectation check_exp(__FILE__, __LINE__, msg);       \
      check_expr;                                                      \
    } else {                                                           \
      check_expr;                                                      \
    }                                                                  \
  } while (0)

class CheckTest : public testing::Test {};

TEST_F(CheckTest, Basics) {
  EXPECT_CHECK("Check failed: false. ", CHECK(false));

  EXPECT_CHECK("Check failed: false. foo", CHECK(false) << "foo");

  double a = 2, b = 1;
  EXPECT_CHECK("Check failed: a < b (2 vs. 1)", CHECK_LT(a, b));

  EXPECT_CHECK("Check failed: a < b (2 vs. 1)foo", CHECK_LT(a, b) << "foo");
}

TEST_F(CheckTest, PCheck) {
  const char file[] = "/nonexistentfile123";
  ignore_result(fopen(file, "r"));
  std::string err =
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());

  EXPECT_CHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " : " +
          err,
      PCHECK(fopen(file, "r") != nullptr));

  EXPECT_CHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " foo: " +
          err,
      PCHECK(fopen(file, "r") != nullptr) << "foo");

  EXPECT_DCHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " : " +
          err,
      DPCHECK(fopen(file, "r") != nullptr));

  EXPECT_DCHECK(
      "Check failed: fopen(file, \"r\") != nullptr."
      " foo: " +
          err,
      DPCHECK(fopen(file, "r") != nullptr) << "foo");
}

TEST_F(CheckTest, CheckOp) {
  int a = 1, b = 2;
  // clang-format off
  EXPECT_CHECK("Check failed: a == b (1 vs. 2)", CHECK_EQ(a, b));
  EXPECT_CHECK("Check failed: a != a (1 vs. 1)", CHECK_NE(a, a));
  EXPECT_CHECK("Check failed: b <= a (2 vs. 1)", CHECK_LE(b, a));
  EXPECT_CHECK("Check failed: b < a (2 vs. 1)",  CHECK_LT(b, a));
  EXPECT_CHECK("Check failed: a >= b (1 vs. 2)", CHECK_GE(a, b));
  EXPECT_CHECK("Check failed: a > b (1 vs. 2)",  CHECK_GT(a, b));

  EXPECT_DCHECK("Check failed: a == b (1 vs. 2)", DCHECK_EQ(a, b));
  EXPECT_DCHECK("Check failed: a != a (1 vs. 1)", DCHECK_NE(a, a));
  EXPECT_DCHECK("Check failed: b <= a (2 vs. 1)", DCHECK_LE(b, a));
  EXPECT_DCHECK("Check failed: b < a (2 vs. 1)",  DCHECK_LT(b, a));
  EXPECT_DCHECK("Check failed: a >= b (1 vs. 2)", DCHECK_GE(a, b));
  EXPECT_DCHECK("Check failed: a > b (1 vs. 2)",  DCHECK_GT(a, b));
  // clang-format on
}

TEST_F(CheckTest, CheckStreamsAreLazy) {
  int called_count = 0;
  int not_called_count = 0;

  auto Called = [&]() {
    ++called_count;
    return 42;
  };
  auto NotCalled = [&]() {
    ++not_called_count;
    return 42;
  };

  CHECK(Called()) << NotCalled();
  CHECK_EQ(Called(), Called()) << NotCalled();
  PCHECK(Called()) << NotCalled();

  DCHECK(Called()) << NotCalled();
  DCHECK_EQ(Called(), Called()) << NotCalled();
  DPCHECK(Called()) << NotCalled();

  EXPECT_EQ(not_called_count, 0);
#if DCHECK_IS_ON()
  EXPECT_EQ(called_count, 8);
#else
  EXPECT_EQ(called_count, 4);
#endif
}

void DcheckEmptyFunction1() {
  // Provide a body so that Release builds do not cause the compiler to
  // optimize DcheckEmptyFunction1 and DcheckEmptyFunction2 as a single
  // function, which breaks the Dcheck tests below.
  LOG(INFO) << "DcheckEmptyFunction1";
}
void DcheckEmptyFunction2() {}

#if defined(DCHECK_IS_CONFIGURABLE)
class ScopedDcheckSeverity {
 public:
  ScopedDcheckSeverity(LogSeverity new_severity) : old_severity_(LOG_DCHECK) {
    LOG_DCHECK = new_severity;
  }

  ~ScopedDcheckSeverity() { LOG_DCHECK = old_severity_; }

 private:
  LogSeverity old_severity_;
};
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// https://crbug.com/709067 tracks test flakiness on iOS.
#if defined(OS_IOS)
#define MAYBE_Dcheck DISABLED_Dcheck
#else
#define MAYBE_Dcheck Dcheck
#endif
TEST_F(CheckTest, MAYBE_Dcheck) {
#if defined(DCHECK_IS_CONFIGURABLE)
  // DCHECKs are enabled, and LOG_DCHECK is mutable, but defaults to non-fatal.
  // Set it to LOG_FATAL to get the expected behavior from the rest of this
  // test.
  ScopedDcheckSeverity dcheck_severity(LOG_FATAL);
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // Release build.
  EXPECT_FALSE(DCHECK_IS_ON());
  EXPECT_FALSE(DLOG_IS_ON(DCHECK));
#elif defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
  // Release build with real DCHECKS.
  EXPECT_TRUE(DCHECK_IS_ON());
  EXPECT_TRUE(DLOG_IS_ON(DCHECK));
#else
  // Debug build.
  EXPECT_TRUE(DCHECK_IS_ON());
  EXPECT_TRUE(DLOG_IS_ON(DCHECK));
#endif

  EXPECT_DCHECK("Check failed: false. ", DCHECK(false));
  std::string err =
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());
  EXPECT_DCHECK("Check failed: false. : " + err, DPCHECK(false));
  EXPECT_DCHECK("Check failed: 0 == 1 (0 vs. 1)", DCHECK_EQ(0, 1));

  // Test DCHECK on std::nullptr_t
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  DCHECK_EQ(p_null, nullptr);
  DCHECK_EQ(nullptr, p_null);
  DCHECK_NE(p_not_null, nullptr);
  DCHECK_NE(nullptr, p_not_null);

  // Test DCHECK on a scoped enum.
  enum class Animal { DOG, CAT };
  DCHECK_EQ(Animal::DOG, Animal::DOG);
  EXPECT_DCHECK("Check failed: Animal::DOG == Animal::CAT (0 vs. 1)",
                DCHECK_EQ(Animal::DOG, Animal::CAT));

  // Test DCHECK on functions and function pointers.
  struct MemberFunctions {
    void MemberFunction1() {
      // See the comment in DcheckEmptyFunction1().
      LOG(INFO) << "Do not merge with MemberFunction2.";
    }
    void MemberFunction2() {}
  };
  void (MemberFunctions::*mp1)() = &MemberFunctions::MemberFunction1;
  void (MemberFunctions::*mp2)() = &MemberFunctions::MemberFunction2;
  void (*fp1)() = DcheckEmptyFunction1;
  void (*fp2)() = DcheckEmptyFunction2;
  void (*fp3)() = DcheckEmptyFunction1;
  DCHECK_EQ(fp1, fp3);
  DCHECK_EQ(mp1, &MemberFunctions::MemberFunction1);
  DCHECK_EQ(mp2, &MemberFunctions::MemberFunction2);
  EXPECT_DCHECK("=~Check failed: fp1 == fp2 \\(\\w+ vs. \\w+\\)",
                DCHECK_EQ(fp1, fp2));
  EXPECT_DCHECK(
      "Check failed: mp2 == &MemberFunctions::MemberFunction1 (1 vs. 1)",
      DCHECK_EQ(mp2, &MemberFunctions::MemberFunction1));
}

TEST_F(CheckTest, DcheckReleaseBehavior) {
  int var1 = 1;
  int var2 = 2;
  int var3 = 3;
  int var4 = 4;

  // No warnings about unused variables even though no check fires and DCHECK
  // may or may not be enabled.
  DCHECK(var1) << var2;
  DPCHECK(var1) << var3;
  DCHECK_EQ(var1, 1) << var4;
}

TEST_F(CheckTest, DCheckEqStatements) {
  bool reached = false;
  if (false)
    DCHECK_EQ(false, true);  // Unreached.
  else
    DCHECK_EQ(true, reached = true);  // Reached, passed.
  ASSERT_EQ(DCHECK_IS_ON() ? true : false, reached);

  if (false)
    DCHECK_EQ(false, true);  // Unreached.
}

TEST_F(CheckTest, CheckEqStatements) {
  bool reached = false;
  if (false)
    CHECK_EQ(false, true);  // Unreached.
  else
    CHECK_EQ(true, reached = true);  // Reached, passed.
  ASSERT_TRUE(reached);

  if (false)
    CHECK_EQ(false, true);  // Unreached.
}

#if defined(DCHECK_IS_CONFIGURABLE)
TEST_F(CheckTest, ConfigurableDCheck) {
  // Verify that DCHECKs default to non-fatal in configurable-DCHECK builds.
  // Note that we require only that DCHECK is non-fatal by default, rather
  // than requiring that it be exactly INFO, ERROR, etc level.
  EXPECT_LT(LOG_DCHECK, LOG_FATAL);
  DCHECK(false);

  // Verify that DCHECK* aren't hard-wired to crash on failure.
  LOG_DCHECK = LOG_INFO;
  DCHECK(false);
  DCHECK_EQ(1, 2);

  // Verify that DCHECK does crash if LOG_DCHECK is set to LOG_FATAL.
  LOG_DCHECK = LOG_FATAL;
  EXPECT_CHECK("Check failed: false. ", DCHECK(false));
  EXPECT_CHECK("Check failed: 1 == 2 (1 vs. 2)", DCHECK_EQ(1, 2));
}

TEST_F(CheckTest, ConfigurableDCheckFeature) {
  // Initialize FeatureList with and without DcheckIsFatal, and verify the
  // value of LOG_DCHECK. Note that we don't require that DCHECK take a
  // specific value when the feature is off, only that it is non-fatal.

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("DcheckIsFatal", "");
    EXPECT_EQ(LOG_DCHECK, LOG_FATAL);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("", "DcheckIsFatal");
    EXPECT_LT(LOG_DCHECK, LOG_FATAL);
  }

  // The default case is last, so we leave LOG_DCHECK in the default state.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine("", "");
    EXPECT_LT(LOG_DCHECK, LOG_FATAL);
  }
}
#endif  // defined(DCHECK_IS_CONFIGURABLE)

struct StructWithOstream {
  bool operator==(const StructWithOstream& o) const { return &o == this; }
};
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::ostream& operator<<(std::ostream& out, const StructWithOstream&) {
  return out << "ostream";
}
#endif

struct StructWithToString {
  bool operator==(const StructWithToString& o) const { return &o == this; }
  std::string ToString() const { return "ToString"; }
};

struct StructWithToStringAndOstream {
  bool operator==(const StructWithToStringAndOstream& o) const {
    return &o == this;
  }
  std::string ToString() const { return "ToString"; }
};
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::ostream& operator<<(std::ostream& out,
                         const StructWithToStringAndOstream&) {
  return out << "ostream";
}
#endif

TEST_F(CheckTest, OstreamVsToString) {
  StructWithOstream a, b;
  EXPECT_CHECK("Check failed: a == b (ostream vs. ostream)", CHECK_EQ(a, b));

  StructWithToString c, d;
  EXPECT_CHECK("Check failed: c == d (ToString vs. ToString)", CHECK_EQ(c, d));

  StructWithToStringAndOstream e, f;
  EXPECT_CHECK("Check failed: e == f (ostream vs. ostream)", CHECK_EQ(e, f));
}

}  // namespace
