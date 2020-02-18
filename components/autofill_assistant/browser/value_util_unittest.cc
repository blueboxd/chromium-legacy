// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace value_util {

class ValueUtilTest : public testing::Test {
 public:
  ValueUtilTest() = default;
  ~ValueUtilTest() override {}

  ValueProto CreateStringValue() const {
    ValueProto value;
    value.mutable_strings()->add_values("Aurea prima");
    value.mutable_strings()->add_values("sata est,");
    value.mutable_strings()->add_values("aetas quae");
    value.mutable_strings()->add_values("vindice nullo");
    value.mutable_strings()->add_values("ü万𠜎");
    return value;
  }

  ValueProto CreateIntValue() const {
    ValueProto value;
    value.mutable_ints()->add_values(1);
    value.mutable_ints()->add_values(123);
    value.mutable_ints()->add_values(5);
    value.mutable_ints()->add_values(-132);
    return value;
  }

  ValueProto CreateBoolValue() const {
    ValueProto value;
    value.mutable_booleans()->add_values(true);
    value.mutable_booleans()->add_values(false);
    value.mutable_booleans()->add_values(true);
    value.mutable_booleans()->add_values(true);
    return value;
  }
};

TEST_F(ValueUtilTest, DifferentTypesComparison) {
  ValueProto value_a;
  ValueProto value_b = CreateStringValue();
  ValueProto value_c = CreateIntValue();
  ValueProto value_d = CreateBoolValue();

  EXPECT_FALSE(value_a == value_b);
  EXPECT_FALSE(value_a == value_c);
  EXPECT_FALSE(value_a == value_d);
  EXPECT_FALSE(value_b == value_c);
  EXPECT_FALSE(value_b == value_d);
  EXPECT_FALSE(value_c == value_d);

  EXPECT_TRUE(value_a == value_a);
  EXPECT_TRUE(value_b == value_b);
  EXPECT_TRUE(value_c == value_c);
  EXPECT_TRUE(value_d == value_d);
}

TEST_F(ValueUtilTest, EmptyValueComparison) {
  ValueProto value_a;
  ValueProto value_b;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_strings()->add_values("potato");
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_strings()->clear_values();
  EXPECT_FALSE(value_a == value_b);

  value_a.clear_kind();
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, StringComparison) {
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_strings()->add_values("potato");
  EXPECT_FALSE(value_a == value_b);

  value_b.mutable_strings()->add_values("tomato");
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_strings()->set_values(value_a.strings().values_size() - 1,
                                        "tomato");
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, IntComparison) {
  ValueProto value_a = CreateIntValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_ints()->add_values(1);
  value_b.mutable_ints()->add_values(0);
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_ints()->set_values(value_a.ints().values_size() - 1, 0);
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, BoolComparison) {
  ValueProto value_a = CreateBoolValue();
  ValueProto value_b = value_a;
  EXPECT_TRUE(value_a == value_b);

  value_a.mutable_booleans()->add_values(true);
  value_b.mutable_booleans()->add_values(false);
  EXPECT_FALSE(value_a == value_b);

  value_a.mutable_booleans()->set_values(value_a.booleans().values_size() - 1,
                                         false);
  EXPECT_TRUE(value_a == value_b);
}

TEST_F(ValueUtilTest, AreAllValuesOfType) {
  ValueProto value_a;
  ValueProto value_b;
  ValueProto value_c;
  EXPECT_TRUE(AreAllValuesOfType({value_a, value_b, value_c},
                                 ValueProto::KIND_NOT_SET));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kBooleans));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kInts));

  value_a = SimpleValue(std::string(""));
  value_b = SimpleValue(std::string("non-empty"));
  EXPECT_TRUE(AreAllValuesOfType({value_a, value_b}, ValueProto::kStrings));
  EXPECT_FALSE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));

  value_c = CreateStringValue();
  EXPECT_TRUE(
      AreAllValuesOfType({value_a, value_b, value_c}, ValueProto::kStrings));
}

TEST_F(ValueUtilTest, AreAllValuesOfSize) {
  // Not-set values have size 0.
  ValueProto value_a;
  ValueProto value_b;
  ValueProto value_c;
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 0));

  value_a = SimpleValue(std::string(""));
  value_b = SimpleValue(std::string("non-empty"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b}, 1));

  value_c = SimpleValue(std::string("another"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 1));

  value_c.mutable_strings()->add_values(std::string("second value"));
  EXPECT_FALSE(AreAllValuesOfSize({value_a, value_b, value_c}, 1));

  value_a.mutable_strings()->add_values(std::string(""));
  value_b.mutable_strings()->add_values(std::string("test"));
  EXPECT_TRUE(AreAllValuesOfSize({value_a, value_b, value_c}, 2));
}

TEST_F(ValueUtilTest, CombineValues) {
  ValueProto value_a;
  ValueProto value_b;
  ValueProto value_c;
  EXPECT_THAT(CombineValues({value_a, value_b, value_c}), ValueProto());

  value_a = SimpleValue(1);
  EXPECT_THAT(CombineValues({value_a, value_b, value_c}), base::nullopt);

  value_a.mutable_strings()->add_values("First");
  value_a.mutable_strings()->add_values("Second");
  value_b.mutable_strings();
  value_c.mutable_strings()->add_values("Third");
  ValueProto expected;
  expected.mutable_strings()->add_values("First");
  expected.mutable_strings()->add_values("Second");
  expected.mutable_strings()->add_values("Third");
  EXPECT_THAT(CombineValues({value_a, value_b, value_c}), expected);

  value_a = ValueProto();
  value_a.mutable_ints();
  value_b = SimpleValue(1);
  value_c = SimpleValue(2);
  value_c.mutable_ints()->add_values(3);
  expected.mutable_ints()->add_values(1);
  expected.mutable_ints()->add_values(2);
  expected.mutable_ints()->add_values(3);
  EXPECT_THAT(CombineValues({value_a, value_b, value_c}), expected);

  value_a = SimpleValue(false);
  value_b = SimpleValue(true);
  value_b.mutable_booleans()->add_values(false);
  value_c = ValueProto();
  value_c.mutable_booleans();
  expected.mutable_booleans()->add_values(false);
  expected.mutable_booleans()->add_values(true);
  expected.mutable_booleans()->add_values(false);
  EXPECT_THAT(CombineValues({value_a, value_b, value_c}), expected);
}

}  // namespace value_util
}  // namespace autofill_assistant
