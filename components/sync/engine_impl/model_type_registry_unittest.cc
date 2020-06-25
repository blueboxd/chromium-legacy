// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_registry.h"

#include <utility>

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/fake_model_type_processor.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/test/engine/fake_model_worker.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class ModelTypeRegistryTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_user_share_.SetUp();
    scoped_refptr<ModelSafeWorker> passive_worker(
        new FakeModelWorker(GROUP_PASSIVE));
    scoped_refptr<ModelSafeWorker> ui_worker(
        new FakeModelWorker(GROUP_NON_BLOCKING));
    workers_.push_back(passive_worker);
    workers_.push_back(ui_worker);

    registry_ = std::make_unique<ModelTypeRegistry>(
        workers_, test_user_share_.user_share(), &mock_nudge_handler_,
        &cancelation_signal_, test_user_share_.keystore_keys_handler());
  }

  void TearDown() override {
    registry_.reset();
    workers_.clear();
    test_user_share_.TearDown();
  }

  ModelTypeRegistry* registry() { return registry_.get(); }

  static sync_pb::ModelTypeState MakeInitialModelTypeState(ModelType type) {
    sync_pb::ModelTypeState state;
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type));
    return state;
  }

  static std::unique_ptr<DataTypeActivationResponse>
  MakeDataTypeActivationResponse(
      const sync_pb::ModelTypeState& model_type_state) {
    auto context = std::make_unique<DataTypeActivationResponse>();
    context->model_type_state = model_type_state;
    context->type_processor = std::make_unique<FakeModelTypeProcessor>();
    return context;
  }

  void SetDummyProgressMarkerForType(ModelType type) {
    sync_pb::DataTypeProgressMarker progress_marker;
    progress_marker.set_token("dummy");
    directory()->SetDownloadProgress(type, progress_marker);
  }

  syncable::MetahandleSet metahandles_to_purge() {
    return directory()->kernel()->metahandles_to_purge;
  }

 private:
  syncable::Directory* directory() {
    return test_user_share_.user_share()->directory.get();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  TestUserShare test_user_share_;
  CancelationSignal cancelation_signal_;
  std::vector<scoped_refptr<ModelSafeWorker>> workers_;
  std::unique_ptr<ModelTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
};

TEST_F(ModelTypeRegistryTest, NonBlockingTypes) {
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->ConnectNonBlockingType(
      THEMES,
      MakeDataTypeActivationResponse(MakeInitialModelTypeState(THEMES)));
  EXPECT_EQ(ModelTypeSet(THEMES), registry()->GetEnabledTypes());

  registry()->ConnectNonBlockingType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialModelTypeState(SESSIONS)));
  EXPECT_EQ(ModelTypeSet(THEMES, SESSIONS), registry()->GetEnabledTypes());

  registry()->DisconnectNonBlockingType(THEMES);
  EXPECT_EQ(ModelTypeSet(SESSIONS), registry()->GetEnabledTypes());

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' ModelTypeSyncWorker.
}

// Tests correct result returned from GetInitialSyncEndedTypes.
TEST_F(ModelTypeRegistryTest, GetInitialSyncEndedTypes) {
  // Themes has finished initial sync.
  sync_pb::ModelTypeState model_type_state = MakeInitialModelTypeState(THEMES);
  model_type_state.set_initial_sync_done(true);
  registry()->ConnectNonBlockingType(
      THEMES, MakeDataTypeActivationResponse(model_type_state));

  // SESSIONS has NOT finished initial sync.
  registry()->ConnectNonBlockingType(
      SESSIONS,
      MakeDataTypeActivationResponse(MakeInitialModelTypeState(SESSIONS)));

  EXPECT_EQ(ModelTypeSet(THEMES), registry()->GetInitialSyncEndedTypes());
}

}  // namespace

}  // namespace syncer
