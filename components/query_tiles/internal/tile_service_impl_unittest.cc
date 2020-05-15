// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/query_tiles/internal/image_prefetcher.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using testing::_;
using ::testing::Invoke;

namespace query_tiles {
namespace {

class MockTileManager : public TileManager {
 public:
  MockTileManager() = default;
  MOCK_METHOD1(Init, void(TileGroupStatusCallback));
  MOCK_METHOD1(GetTiles, void(GetTilesCallback));
  MOCK_METHOD2(GetTile, void(const std::string&, TileCallback));
  MOCK_METHOD2(SaveTiles,
               void(std::unique_ptr<TileGroup>, TileGroupStatusCallback));
  MOCK_METHOD1(SetAcceptLanguagesForTesting, void(const std::string&));
};

class MockTileServiceScheduler : public TileServiceScheduler {
 public:
  MockTileServiceScheduler() = default;
  MOCK_METHOD1(OnFetchCompleted, void(TileInfoRequestStatus));
  MOCK_METHOD1(OnTileManagerInitialized, void(TileGroupStatus));
  MOCK_METHOD0(CancelTask, void());
};

class MockImagePrefetcher : public ImagePrefetcher {
 public:
  MockImagePrefetcher() = default;
  ~MockImagePrefetcher() override = default;

  MOCK_METHOD(void, Prefetch, (TileGroup, bool, base::OnceClosure), (override));
};

}  // namespace

class TileServiceImplTest : public testing::Test {
 public:
  TileServiceImplTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~TileServiceImplTest() override = default;

  void SetUp() override {
    auto tile_manager = std::make_unique<MockTileManager>();
    tile_manager_ = tile_manager.get();
    auto image_prefetcher = std::make_unique<MockImagePrefetcher>();
    auto scheduler = std::make_unique<MockTileServiceScheduler>();
    scheduler_ = scheduler.get();
    image_prefetcher_ = image_prefetcher.get();
    ON_CALL(*image_prefetcher_, Prefetch(_, _, _))
        .WillByDefault(Invoke([](TileGroup, bool, base::OnceClosure callback) {
          std::move(callback).Run();
        }));

    auto tile_fetcher =
        TileFetcher::Create(GURL("https://www.test.com"), "US", "en", "apikey",
                            "", test_shared_url_loader_factory_);
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
          last_resource_request_ = request;
        }));
    tile_service_impl_ = std::make_unique<TileServiceImpl>(
        std::move(image_prefetcher), std::move(tile_manager),
        std::move(scheduler), std::move(tile_fetcher), &clock_);
  }

  void Initialize(bool expected_result) {
    base::RunLoop loop;
    tile_service_impl_->Initialize(base::BindOnce(
        &TileServiceImplTest::OnInitialized, base::Unretained(this),
        loop.QuitClosure(), expected_result));
    loop.Run();
  }

  void OnInitialized(base::RepeatingClosure closure,
                     bool expected_result,
                     bool success) {
    EXPECT_EQ(expected_result, success);
    std::move(closure).Run();
  }

  void FetchForTilesSuceeded() {
    EXPECT_CALL(*scheduler(),
                OnFetchCompleted(TileInfoRequestStatus::kSuccess));
    tile_service_impl_->StartFetchForTiles(
        false, base::BindOnce(&TileServiceImplTest::OnFetchCompleted,
                              base::Unretained(this)));
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        last_resource_request_.url.spec(), "", net::HTTP_OK,
        network::TestURLLoaderFactory::kMostRecentMatch);
    task_environment_.RunUntilIdle();
  }

  void FetchForTilesWithError() {
    EXPECT_CALL(*scheduler(),
                OnFetchCompleted(TileInfoRequestStatus::kShouldSuspend));
    tile_service_impl_->StartFetchForTiles(
        false, base::BindOnce(&TileServiceImplTest::OnFetchCompleted,
                              base::Unretained(this)));
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        last_resource_request_.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_FORBIDDEN), std::string(),
        network::TestURLLoaderFactory::kMostRecentMatch);
    task_environment_.RunUntilIdle();
  }

  void OnFetchCompleted(bool reschedule) {
    // Once fetch completes, no reschedule should happen.
    EXPECT_FALSE(reschedule);
  }

  MockTileServiceScheduler* scheduler() { return scheduler_; }
  MockTileManager* tile_manager() { return tile_manager_; }
  MockImagePrefetcher* image_prefetcher() { return image_prefetcher_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  std::unique_ptr<TileServiceImpl> tile_service_impl_;
  MockTileServiceScheduler* scheduler_;
  MockTileManager* tile_manager_;
  MockImagePrefetcher* image_prefetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;
};

// Tests that TileManager and TileServiceImpl both initialize successfully.
TEST_F(TileServiceImplTest, ManagerInitSucceeded) {
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kSuccess);
      }));
  Initialize(true);
}

// Tests that TileManager and TileServiceImpl both initialize successfully with
// no tiles.
TEST_F(TileServiceImplTest, ManagerInitSucceededWithNoTiles) {
  EXPECT_CALL(*scheduler(),
              OnTileManagerInitialized(TileGroupStatus::kNoTiles));
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kNoTiles);
      }));
  Initialize(true);
}

// Tests that TileManager fail to init, that causes TileServiceImpl to fail to
// initialize too.
TEST_F(TileServiceImplTest, ManagerInitFailed) {
  EXPECT_CALL(*tile_manager(), Init(_))
      .WillOnce(Invoke([](base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      }));
  Initialize(false);
}

// Tests that tiles are successfully fetched and saved.
TEST_F(TileServiceImplTest, FetchForTilesSucceeded) {
  EXPECT_CALL(*tile_manager(), SaveTiles(_, _))
      .WillOnce(Invoke([](std::unique_ptr<TileGroup> tile_group,
                          base::OnceCallback<void(TileGroupStatus)> callback) {
        std::move(callback).Run(TileGroupStatus::kSuccess);
      }));
  EXPECT_CALL(*image_prefetcher(), Prefetch(_, _, _));

  FetchForTilesSuceeded();
}

// Tests that tiles failed to fetch.
TEST_F(TileServiceImplTest, FetchForTilesFailed) {
  FetchForTilesWithError();
}

}  // namespace query_tiles
