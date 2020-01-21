// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_bridge_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

// Action SaveArgPointeeMove<k>(pointer) saves the value pointed to by the k-th
// (0-based) argument of the mock function by moving it to *pointer.
ACTION_TEMPLATE(SaveArgPointeeMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(*testing::get<k>(args));
}

class SharingMessageBridgeTest : public testing::Test {
 protected:
  SharingMessageBridgeTest() {
    EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
    bridge_ = std::make_unique<SharingMessageBridgeImpl>(
        mock_processor_.CreateForwardingProcessor());
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  }

  SharingMessageBridgeImpl* bridge() { return bridge_.get(); }
  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  std::unique_ptr<sync_pb::SharingMessageSpecifics> CreateSpecifics(
      const std::string& payload) const {
    auto specifics = std::make_unique<sync_pb::SharingMessageSpecifics>();
    specifics->set_payload(payload);
    return specifics;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<SharingMessageBridgeImpl> bridge_;
};

TEST_F(SharingMessageBridgeTest, ShouldWriteMessagesToProcessor) {
  syncer::EntityData entity_data;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillRepeatedly(SaveArgPointeeMove<1>(&entity_data));
  bridge()->SendSharingMessage(CreateSpecifics("test_payload"));

  EXPECT_TRUE(entity_data.specifics.has_sharing_message());
  EXPECT_EQ(entity_data.specifics.sharing_message().payload(), "test_payload");

  entity_data.specifics.Clear();
  bridge()->SendSharingMessage(CreateSpecifics("another_payload"));

  EXPECT_TRUE(entity_data.specifics.has_sharing_message());
  EXPECT_EQ(entity_data.specifics.sharing_message().payload(),
            "another_payload");
}

TEST_F(SharingMessageBridgeTest, ShouldNotGenerateStorageKey) {
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _)).WillOnce(SaveArg<0>(&storage_key));
  bridge()->SendSharingMessage(
      std::make_unique<sync_pb::SharingMessageSpecifics>());

  EXPECT_TRUE(storage_key.empty());
}

}  // namespace
