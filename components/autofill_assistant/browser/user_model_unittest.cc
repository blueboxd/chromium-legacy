// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/mock_user_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class UserModelTest : public testing::Test {
 public:
  UserModelTest() = default;
  ~UserModelTest() override {}

  void SetUp() override { model_.AddObserver(&mock_observer_); }

  void TearDown() override { model_.RemoveObserver(&mock_observer_); }

  // Provides direct access to the values in the model for testing.
  const std::map<std::string, ValueProto>& GetValues() const {
    return model_.values_;
  }

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

 protected:
  UserModel model_;
  MockUserModelObserver mock_observer_;
};

TEST_F(UserModelTest, EmptyValue) {
  ValueProto value;
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", value)).Times(1);
  model_.SetValue("identifier", value);
  model_.SetValue("identifier", value);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("identifier", value)));
}

TEST_F(UserModelTest, InsertNewValues) {
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = CreateIntValue();
  ValueProto value_c = CreateBoolValue();

  testing::InSequence seq;
  EXPECT_CALL(mock_observer_, OnValueChanged("value_a", value_a)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("value_b", value_b)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("value_c", value_c)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged(_, _)).Times(0);

  model_.SetValue("value_a", value_a);
  model_.SetValue("value_b", value_b);
  model_.SetValue("value_c", value_c);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("value_a", value_a),
                                                Pair("value_b", value_b),
                                                Pair("value_c", value_c)));
}

TEST_F(UserModelTest, OverwriteWithExistingValueFiresNoChangeEvent) {
  ValueProto value = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", value)).Times(1);
  model_.SetValue("identifier", value);

  ValueProto same_value = CreateStringValue();
  model_.SetValue("identifier", same_value);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(Pair("identifier", value)));
}

TEST_F(UserModelTest, OverwriteWithDifferentValueFiresChangeEvent) {
  ValueProto value = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("identifier", _)).Times(2);
  model_.SetValue("identifier", value);

  ValueProto another_value = CreateStringValue();
  another_value.mutable_strings()->add_values("tomato");
  model_.SetValue("identifier", another_value);

  EXPECT_THAT(GetValues(),
              UnorderedElementsAre(Pair("identifier", another_value)));
}

TEST_F(UserModelTest, ForceNotificationAlwaysFiresChangeEvent) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  model_.SetValue("a", value_a);

  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(0);
  model_.SetValue("a", value_a);

  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  model_.SetValue("a", value_a, /* force_notification = */ true);
}

TEST_F(UserModelTest, MergeWithProto) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  ValueProto value_b = CreateIntValue();
  ValueProto value_d = CreateBoolValue();
  EXPECT_CALL(mock_observer_, OnValueChanged("a", value_a)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("b", value_b)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("c", ValueProto())).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("d", value_d)).Times(1);
  model_.SetValue("a", value_a);
  model_.SetValue("b", value_b);
  model_.SetValue("c", ValueProto());
  model_.SetValue("d", value_d);

  ModelProto proto;
  ValueProto value_b_changed = value_b;
  value_b_changed.mutable_ints()->add_values(14);
  ValueProto value_c_changed = CreateBoolValue();
  ValueProto value_e = CreateStringValue();
  // Overwrite existing value.
  auto* value = proto.add_values();
  value->set_identifier("b");
  *value->mutable_value() = value_b_changed;
  // Overwrite existing empty value with non-empty value.
  value = proto.add_values();
  value->set_identifier("c");
  *value->mutable_value() = value_c_changed;
  // Does not overwrite existing non-empty value.
  value = proto.add_values();
  value->set_identifier("d");
  *value->mutable_value() = ValueProto();
  // Inserts new non-empty value.
  value = proto.add_values();
  value->set_identifier("e");
  *value->mutable_value() = value_e;
  // Inserts new empty value.
  value = proto.add_values();
  value->set_identifier("f");
  *value->mutable_value() = ValueProto();

  EXPECT_CALL(mock_observer_, OnValueChanged("a", _)).Times(0);
  EXPECT_CALL(mock_observer_, OnValueChanged("b", value_b_changed)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("c", value_c_changed)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("d", _)).Times(0);
  EXPECT_CALL(mock_observer_, OnValueChanged("e", value_e)).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged("f", ValueProto())).Times(1);
  EXPECT_CALL(mock_observer_, OnValueChanged(_, _)).Times(0);
  model_.MergeWithProto(proto, /*force_notification=*/false);

  EXPECT_THAT(GetValues(), UnorderedElementsAre(
                               Pair("a", value_a), Pair("b", value_b_changed),
                               Pair("c", value_c_changed), Pair("d", value_d),
                               Pair("e", value_e), Pair("f", ValueProto())));
}

TEST_F(UserModelTest, UpdateProto) {
  testing::InSequence seq;
  ValueProto value_a = CreateStringValue();
  ValueProto value_c = CreateBoolValue();
  model_.SetValue("a", value_a);
  model_.SetValue("b", ValueProto());
  model_.SetValue("c", value_c);
  model_.SetValue("d", CreateStringValue());

  ModelProto proto;
  // 'a' in proto is non-empty value and should be overwritten.
  auto* value = proto.add_values();
  value->set_identifier("a");
  *value->mutable_value() = CreateBoolValue();
  // 'b' in proto is non-empty and should be overwritten with empty value.
  value = proto.add_values();
  value->set_identifier("b");
  *value->mutable_value() = CreateIntValue();
  // 'c' in proto is default empty and should be overwritten with |value_c|.
  proto.add_values()->set_identifier("c");
  // 'd' is not in the proto and should not be added to the proto.

  model_.UpdateProto(&proto);
  EXPECT_THAT(
      proto.values(),
      UnorderedElementsAre(
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("a")),
                Property(&ModelProto::ModelValue::value, Eq(value_a))),
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("b")),
                Property(&ModelProto::ModelValue::value, Eq(ValueProto()))),
          AllOf(Property(&ModelProto::ModelValue::identifier, StrEq("c")),
                Property(&ModelProto::ModelValue::value, Eq(value_c)))));
}

}  // namespace autofill_assistant
