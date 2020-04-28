// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/change_record.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::ExpectDictDictionaryValue;
using base::ExpectDictStringValue;

class ChangeRecordTest : public testing::Test {};

void ExpectChangeRecordActionValue(ChangeRecord::Action expected_value,
                                   const base::DictionaryValue& value,
                                   const std::string& key) {
  std::string str_value;
  EXPECT_TRUE(value.GetString(key, &str_value));
  switch (expected_value) {
    case ChangeRecord::ACTION_ADD:
      EXPECT_EQ("Add", str_value);
      break;
    case ChangeRecord::ACTION_UPDATE:
      EXPECT_EQ("Update", str_value);
      break;
    case ChangeRecord::ACTION_DELETE:
      EXPECT_EQ("Delete", str_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CheckChangeRecordValue(const ChangeRecord& record,
                            const base::DictionaryValue& value) {
  ExpectChangeRecordActionValue(record.action, value, "action");
  ExpectDictStringValue(base::NumberToString(record.id), value, "id");
  if (record.action == ChangeRecord::ACTION_DELETE) {
    std::unique_ptr<base::DictionaryValue> expected_extra_value;
    if (record.extra.has_value()) {
      expected_extra_value = record.extra->ToValue();
    }
    const base::Value* extra_value = nullptr;
    EXPECT_EQ(record.extra.has_value(), value.Get("extra", &extra_value));
    if (extra_value)
      EXPECT_EQ(*expected_extra_value, *extra_value);

    std::unique_ptr<base::DictionaryValue> expected_specifics_value(
        EntitySpecificsToValue(record.specifics));
    ExpectDictDictionaryValue(*expected_specifics_value, value, "specifics");
  }
}

TEST_F(ChangeRecordTest, ChangeRecordToValue) {
  sync_pb::EntitySpecifics old_specifics;
  old_specifics.mutable_extension()->set_id("old");
  sync_pb::EntitySpecifics new_specifics;
  old_specifics.mutable_extension()->set_id("new");

  const int64_t kTestId = 5;

  // Add
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_ADD;
    record.id = kTestId;
    record.specifics = old_specifics;
    record.extra.emplace();
    std::unique_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Update
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_UPDATE;
    record.id = kTestId;
    record.specifics = old_specifics;
    record.extra.emplace();
    std::unique_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Delete (no extra)
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_DELETE;
    record.id = kTestId;
    record.specifics = old_specifics;
    std::unique_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Delete (with extra)
  {
    sync_pb::PasswordSpecificsData unencrypted;
    unencrypted.set_origin("http://example.com");
    unencrypted.set_username_value("azure");
    unencrypted.set_password_value("hunter2");

    ChangeRecord record;
    record.action = ChangeRecord::ACTION_DELETE;
    record.id = kTestId;
    record.specifics = old_specifics;
    record.extra = ExtraPasswordChangeRecordData(unencrypted);

    std::unique_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }
}

}  // namespace
}  // namespace syncer
