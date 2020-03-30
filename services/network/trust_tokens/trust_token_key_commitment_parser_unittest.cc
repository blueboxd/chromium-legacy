// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

namespace network {

namespace {

// For convenience, define a matcher for checking mojo struct pointer equality
// in order to simplify validating results returning containers of points.
template <typename T>
decltype(auto) EqualsMojo(const mojo::StructPtr<T>& value) {
  return Truly([&](const mojo::StructPtr<T>& candidate) {
    return mojo::Equals(candidate, value);
  });
}

}  // namespace

TEST(TrustTokenKeyCommitmentParsing, RejectsEmpty) {
  // If the input isn't valid JSON, we should
  // reject it. In particular, we should reject
  // empty input.

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(""));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const char input[] = "certainly not valid JSON";

  // Sanity check that the input is not valid JSON.
  ASSERT_FALSE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsNonDictionaryInput) {
  // The outermost value should be a dictionary.

  // Valid JSON, but not a dictionary.
  const char input[] = "5";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, AcceptsMinimal) {
  std::string input = R"( { "srrkey": "aaaa" } )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  base::Base64Decode("aaaa",
                     &expectation->signed_redemption_record_verification_key);

  EXPECT_THAT(TrustTokenKeyCommitmentParser().Parse(input),
              EqualsMojo(expectation));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsMissingSrrkey) {
  std::string input = R"( {} )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParsing, RejectsTypeUnsafeSrrkey) {
  std::string input = R"( { "srrkey": 5 } )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParsing, RejectsNonBase64Srrkey) {
  std::string input = R"( { "srrkey": "spaces aren't valid base64" } )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithTypeUnsafeKeyLabel) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  // (The expiry will likely exceed the JSON spec's maximum integer value, so
  // it's encoded as a string.)
  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "this label is not an integer": {
              "Y": "akey",
              "expiry": "%s"
            }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithKeyLabelTooSmall) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "-1": {
              "Y": "akey",
              "expiry": "%s"
            }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithKeyLabelTooLarge) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "1000000000000": {
              "Y": "akey",
              "expiry": "%s"
            }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsOtherwiseValidButNonBase64Key) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "1": {
              "Y": "this key isn't valid base64, so it should be rejected",
              "expiry": "%s"
            }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, AcceptsKeyWithExpiryAndBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "1": { "Y": "akey", "expiry": "%s" }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto my_key = mojom::TrustTokenVerificationKey::New();
  my_key->label = 1;
  ASSERT_TRUE(base::Base64Decode("akey", &my_key->body));
  my_key->expiry = one_minute_from_now;

  auto result = TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->keys, ElementsAre(EqualsMojo(my_key)));
}

TEST(TrustTokenKeyCommitmentParsing, AcceptsMultipleKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  base::Time two_minutes_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(2);
  int64_t two_minutes_from_now_in_micros =
      (two_minutes_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({
            "srrkey": "aaaa",
            "1": { "Y": "akey", "expiry": "%s" },
            "2": { "Y": "aaaa", "expiry": "%s" }
         })",
      base::NumberToString(one_minute_from_now_in_micros).c_str(),
      base::NumberToString(two_minutes_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto a_key = mojom::TrustTokenVerificationKey::New();
  a_key->label = 1;
  ASSERT_TRUE(base::Base64Decode("akey", &a_key->body));
  a_key->expiry = one_minute_from_now;

  auto another_key = mojom::TrustTokenVerificationKey::New();
  another_key->label = 2;
  ASSERT_TRUE(base::Base64Decode("aaaa", &another_key->body));
  another_key->expiry = two_minutes_from_now;

  auto result = TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->keys,
              UnorderedElementsAre(EqualsMojo(a_key), EqualsMojo(another_key)));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithNoExpiry) {
  // If a key has a missing "expiry" field, we should reject the entire
  // record.
  const std::string input = R"( { "srrkey": "aaaa",
                                  "1": { "Y": "akey" } })";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithMalformedExpiry) {
  // If a key has a malformed "expiry" field, we should reject the entire
  // record.
  const std::string input =
      R"(
   {
     "srrkey": "aaaa",
     "1": {
       "Y": "akey",
       "expiry": "absolutely not a valid timestamp"
     }
   })";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, IgnoreKeyWithExpiryInThePast) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Ensure that "one minute ago" yields a nonnegative number of microseconds
  // past the Unix epoch.
  env.AdvanceClock(std::max<base::TimeDelta>(
      base::TimeDelta(), base::Time::UnixEpoch() +
                             base::TimeDelta::FromMinutes(1) -
                             base::Time::Now()));

  base::Time one_minute_before_now =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);
  int64_t one_minute_before_now_in_micros =
      (one_minute_before_now - base::Time::UnixEpoch()).InMicroseconds();

  // If the time has passed a key's "expiry" field, we should reject the entire
  // record.
  const std::string input = base::StringPrintf(
      R"( { "srrkey": "aaaa",
            "1": { "Y": "akey", "expiry": "%s" } })",
      base::NumberToString(one_minute_before_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  base::Base64Decode("aaaa",
                     &expectation->signed_redemption_record_verification_key);

  EXPECT_TRUE(
      mojo::Equals(TrustTokenKeyCommitmentParser().Parse(input), expectation));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsKeyWithNoBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now =
      base::Time::Now() + base::TimeDelta::FromMinutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  // If a key has an expiry but is missing its body,
  // we should reject the entire result.
  const std::string input = base::StringPrintf(
      R"( { "srrkey": "aaaa", "1": { "expiry": "%s" } } )",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the date is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have a body, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, RejectsEmptyKey) {
  // If a key has neither an expiry or a body,
  // we should reject the entire result.

  const std::string input = R"( { "srrkey": "aaaa", "1": { } })";

  // Sanity check that the input is actually valid JSON,
  // and that the date is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry or a body, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParsing, ParsesBatchSize) {
  std::string input =
      R"({
     "srrkey": "aaaa",
     "batchsize": 5
   })";

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->batch_size);
  EXPECT_EQ(result->batch_size->value, 5);
}

TEST(TrustTokenKeyCommitmentParsing, RejectsTypeUnsafeBatchSize) {
  std::string input =
      R"({
     "srrkey": "aaaa",
     "batchsize": "not a number"
   })";

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

}  // namespace network
