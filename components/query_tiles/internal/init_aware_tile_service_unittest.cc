// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/init_aware_tile_service.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "components/query_tiles/internal/tile_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::StrictMock;

namespace upboarding {
namespace {

class MockInitializableTileService : public InitializableTileService {
 public:
  MockInitializableTileService() = default;
  ~MockInitializableTileService() override = default;

  void Initialize(SuccessCallback callback) override {
    init_callback_ = std::move(callback);
  }

  void InvokeInitCallback(bool success) {
    DCHECK(init_callback_);
    std::move(init_callback_).Run(success);
  }

  MOCK_METHOD1(GetQueryTiles, void(GetTilesCallback));
  MOCK_METHOD2(GetTile, void(const std::string&, TileCallback));
  MOCK_METHOD1(StartFetchForTiles, void(BackgroundTaskFinishedCallback));

 private:
  SuccessCallback init_callback_;
};

class InitAwareTileServiceTest : public testing::Test {
 public:
  InitAwareTileServiceTest() : mock_service_(nullptr) {}
  ~InitAwareTileServiceTest() override = default;

 protected:
  void SetUp() override {
    auto mock_service =
        std::make_unique<StrictMock<MockInitializableTileService>>();
    mock_service_ = mock_service.get();
    init_aware_service_ =
        std::make_unique<InitAwareTileService>(std::move(mock_service));
  }

 protected:
  TileService* init_aware_service() {
    DCHECK(init_aware_service_);
    return init_aware_service_.get();
  }

  MockInitializableTileService* mock_service() {
    DCHECK(mock_service_);
    return mock_service_;
  }

  void InvokeInitCallback(bool success) {
    mock_service_->InvokeInitCallback(success);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockInitializableTileService* mock_service_;
  std::unique_ptr<InitAwareTileService> init_aware_service_;
};

// API calls invoked after successful initialization should just pass through.
TEST_F(InitAwareTileServiceTest, AfterInitSuccessPassThrough) {
  InvokeInitCallback(true /*success*/);
  InSequence sequence;
  EXPECT_CALL(*mock_service(), GetQueryTiles(_));
  EXPECT_CALL(*mock_service(), GetTile(_, _));
  EXPECT_CALL(*mock_service(), StartFetchForTiles(_));

  init_aware_service()->GetQueryTiles(base::DoNothing());
  init_aware_service()->GetTile("id", base::DoNothing());
  init_aware_service()->StartFetchForTiles(base::DoNothing());
}

// API calls invoked after failed initialization should not pass through.
TEST_F(InitAwareTileServiceTest, AfterInitFailureNotPassThrough) {
  InvokeInitCallback(false /*success*/);
  InSequence sequence;
  EXPECT_CALL(*mock_service(), GetQueryTiles(_)).Times(0);
  EXPECT_CALL(*mock_service(), GetTile(_, _)).Times(0);
  EXPECT_CALL(*mock_service(), StartFetchForTiles(_)).Times(0);

  init_aware_service()->GetQueryTiles(base::DoNothing());
  init_aware_service()->GetTile("id", base::DoNothing());
  init_aware_service()->StartFetchForTiles(base::DoNothing());
}

// API calls invoked before successful initialization should be flushed through.
TEST_F(InitAwareTileServiceTest, BeforeInitSuccessFlushedThrough) {
  InSequence sequence;
  EXPECT_CALL(*mock_service(), GetQueryTiles(_));
  EXPECT_CALL(*mock_service(), GetTile(_, _));
  EXPECT_CALL(*mock_service(), StartFetchForTiles(_));

  init_aware_service()->GetQueryTiles(base::DoNothing());
  init_aware_service()->GetTile("id", base::DoNothing());
  init_aware_service()->StartFetchForTiles(base::DoNothing());
  InvokeInitCallback(true /*success*/);
}

// API calls invoked before failed initialization should not be flushed through.
TEST_F(InitAwareTileServiceTest, BeforeInitFailureNotFlushedThrough) {
  InSequence sequence;
  EXPECT_CALL(*mock_service(), GetQueryTiles(_)).Times(0);
  EXPECT_CALL(*mock_service(), GetTile(_, _)).Times(0);
  EXPECT_CALL(*mock_service(), StartFetchForTiles(_)).Times(0);

  init_aware_service()->GetQueryTiles(base::DoNothing());
  init_aware_service()->GetTile("id", base::DoNothing());
  init_aware_service()->StartFetchForTiles(base::DoNothing());
  InvokeInitCallback(false /*success*/);
}

}  // namespace
}  // namespace upboarding
