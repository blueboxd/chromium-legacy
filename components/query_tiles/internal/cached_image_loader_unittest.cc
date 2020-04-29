// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/cached_image_loader.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

using image_fetcher::ImageDataFetcherCallback;
using image_fetcher::ImageFetcherCallback;
using image_fetcher::ImageFetcherParams;
using image_fetcher::RequestMetadata;
using ::testing::_;
using ::testing::Invoke;

namespace upboarding {
namespace {

class CachedImageLoaderTest : public testing::Test {
 public:
  CachedImageLoaderTest() = default;
  ~CachedImageLoaderTest() override = default;

  void SetUp() override {
    image_loader_ = std::make_unique<CachedImageLoader>(
        &mock_fetcher_, &mock_reduced_mode_fetcher_);
  }

 protected:
  void FetchImage() {
    image_loader_->FetchImage(
        GURL("https://www.example.com/dummy_image"),
        base::BindOnce(&CachedImageLoaderTest::OnBitmapFetched,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void PrefetchImage(int http_response_code, bool expected_success) {
    base::MockCallback<ImageLoader::SuccessCallback> mock_callback;
    EXPECT_CALL(mock_callback, Run(expected_success));
    EXPECT_CALL(*mock_reduced_mode_fetcher(), FetchImageAndData_(_, _, _, _))
        .WillRepeatedly(
            Invoke([http_response_code](
                       const GURL&, ImageDataFetcherCallback* data_callback,
                       ImageFetcherCallback*, ImageFetcherParams) {
              RequestMetadata request_metadata;
              request_metadata.http_response_code = http_response_code;
              std::move(*data_callback).Run("test_data", request_metadata);
            }));
    image_loader_->PrefetchImage(GURL("https://www.example.com/dummy_image"),
                                 mock_callback.Get());
  }

  image_fetcher::MockImageFetcher* mock_fetcher() { return &mock_fetcher_; }

  image_fetcher::MockImageFetcher* mock_reduced_mode_fetcher() {
    return &mock_reduced_mode_fetcher_;
  }

  const SkBitmap& result() const { return result_; }

 private:
  void OnBitmapFetched(SkBitmap bitmap) { result_ = bitmap; }

  base::test::TaskEnvironment task_environment_;
  image_fetcher::MockImageFetcher mock_fetcher_;
  image_fetcher::MockImageFetcher mock_reduced_mode_fetcher_;

  std::unique_ptr<ImageLoader> image_loader_;
  SkBitmap result_;

  base::WeakPtrFactory<CachedImageLoaderTest> weak_ptr_factory_{this};
};

TEST_F(CachedImageLoaderTest, FetchImage) {
  // Create a non-empty bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 16);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(bitmap.width(), 32);
  auto image = gfx::Image::CreateFrom1xBitmap(bitmap);

  EXPECT_CALL(*mock_fetcher(), FetchImageAndData_(_, _, _, _))
      .WillRepeatedly(Invoke([&image](const GURL&, ImageDataFetcherCallback*,
                                      ImageFetcherCallback* fetch_callback,
                                      ImageFetcherParams) {
        std::move(*fetch_callback).Run(image, image_fetcher::RequestMetadata());
      }));
  FetchImage();
  EXPECT_FALSE(result().empty());
  EXPECT_EQ(result().width(), 32);
}

TEST_F(CachedImageLoaderTest, PrefetchImage) {
  PrefetchImage(net::OK, true /*expected_succes*/);
  PrefetchImage(net::HTTP_NOT_FOUND, false /*expected_succes*/);
}

}  // namespace
}  // namespace upboarding
